# 字体发现与渲染

游戏脚本继续使用 `Font`、`Bitmap#text_size` 和 `Bitmap#draw_text`。字体后端、排版缓存、字形图集和字体着色器均位于 `core/font`，不引入 Skia；借鉴其“排版与栅格化分离、覆盖率字形缓存、GPU 合成”的设计。画布只接收通用的三角形批次和提交前准备回调。

## 抗锯齿与尺寸

| 平台 | 字体后端 |
| --- | --- |
| macOS | CoreText 排版，CoreGraphics 灰度覆盖率栅格化 |
| Windows / Linux / Android / WASM | 仓库内 FreeType，轮廓优先与灰度栅格化 |

普通文字优先使用字体轮廓，跳过 SimHei 等字体中常见的内嵌单色位图，避免这些位图使灰度抗锯齿失效。描边同样由轮廓生成；透明文字使用单通道覆盖率，不使用会在彩色背景上产生色边的 LCD 子像素抗锯齿。`Font.solid` 仍显式选择单色输出。只有位图、没有轮廓的字体保留位图回退。

RGSS 默认设置按游戏版本初始化，脚本之后修改 `Font.default_*` 仍然有效：

| 版本 | 字号 | 描边 | 阴影 |
| --- | --- | --- | --- |
| RGSS1 | 22 | 关闭 | 关闭 |
| RGSS2 | 20 | 关闭 | 开启 |
| RGSS3 | 24 | 开启 | 关闭 |

字号在游戏 Bitmap 的像素空间解释，不随显示 DPI 改变。URGE 为兼容原版度量，使用字体 Windows ascent/descent（OpenType `OS/2` 表中的 `usWinAscent` / `usWinDescent`）将请求字号换算为 em；缺失时回退到后端的 ascent/descent，line gap 不计入行框。这是后端的兼容策略，RGSS 手册没有公布原版字体映射器的内部参数，不能由 GDI 的一般文档直接推出原版的完整实现。

不使用统一的 0.9 系数，也不把字号直接当作 em。SimHei 的 Windows 字符单元恰为 1 em；萝莉体等字符单元高于 1 em 的字体需按自身度量换算。FreeType 的测量、kerning 与栅格化共用精确的 em 换算比例，不先把 em 取成整数。ascent、descent 分别四舍五入后相加得到字符单元高度；例如黑体32号的字符单元高度为33。`Font.size` 和脚本行高不变，实际笔画高度也不等于字符单元高度。

有原生 hint 指令的字体保留原生 hint 度量；无指令或明确关闭 hint 的字体，使用 FreeType 的16.16高精度线性 advance，再按游戏像素四舍五入。测量不使用 FreeType 自动 hint 推导的字距，也不提前量化为26.6后再次取整；合成粗体在取整后增加1像素 advance。栅格化继续使用 `gasp` / 灰度抗锯齿策略。测量和绘制使用同一组笔位置，不在逐字测量和整段测量中分别重复向上取整。`text_size` 包含普通字形的负左边距，不包含描边、阴影或额外斜体伸出。比如萝莉体18号的 `"3"`、`"33"`、`"333"` 分别为7、13、19像素；行首的负边距只计入一次，不用于改变内部字符的笔位置。Kerning、连字等上下文排版也可能使整段宽度不同于单字宽度之和。

### `Bitmap#draw_text` 的布局契约

应分清三种尺寸：`text_size` 返回的逻辑文字尺寸、参与对齐和缩放的文字层尺寸，以及轮廓栅格化后的着墨范围。文字层保留逻辑宽度，同时容纳实际填充像素的左右伸出；普通字形也可能有负左边距或右侧超出 advance。绘制原点按实际填充的负左边距补偿，负边距不能重复加入层宽；斜体伸出也由同一流程处理。这些绘制范围不改变脚本取得的逻辑宽度或内部字符的推进位置。

- 两个 `draw_text` 重载使用相同规则。超宽时只横向压缩，最小比例为60%；不放大，不改变字号、测量值或换行。
- 无描边时，文字层宽度取逻辑宽度与补偿负左边距后的填充范围的较大值；高度使用字符单元高度。RGSS3 描边文字层的宽高各增加2像素；这不加进 `text_size`。阴影不再额外改变文字层尺寸。
- 横向缩放使用 `max(0.6, min(1.0, rect.width / layer_width))`。左/中/右对齐使用缩放后的文字层宽度，不依赖某个字形的实际笔画边缘。普通文字居中的半像素向下取整，描边文字居中的半像素向上取整。最小压缩比例下仍超宽时，从框左端裁切。
- 垂直位置为 `max(0, floor((rect.height - layer_height) / 2))`。行框比文字层矮时从框顶开始绘制，再按行框裁切；描边使文字层增高时也不能把整行文字向上推。不做垂直缩放。
- 文字层先约束自身字形的绘制范围，再与传入矩形和 Bitmap 求交裁切。负边距不能因文字居中或居右而突然露出不同的笔画；裁切后在边缘的一个源像素宽度内重建描边封口。Windows 原版样本中，填充刚好进入第一列时，这一列同样会保留描边颜色；不能通过整体右移字形来补偿。
- 对齐仍以调用者传入的矩形为准；矩形超出 Bitmap 时不自动改写为相交后的宽度再对齐。`Window` 的 padding 是独立的内容布局边距，不属于 Bitmap 文字层。

### 依据与验证边界

[RGSS1 手册](https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_bitmap.html)、[RGSS2 手册](https://www.rpg-maker.fr/dl/monos/aide/vx/source/rgss/gc_bitmap.html)和[RGSS3 手册转载](https://pdfcoffee.com/rpg-maker-vx-ace-help-rgss3-reference-manual-4-pdf-free.html)规定了横向缩小、对齐、垂直居中以及 `text_size` 排除斜体伸出和描边的语义。上面的描边文字层增量、半像素取整和阴影不参与对齐，是原版运行时的行为对照结果，不能说是手册公开了内部算法，也不意味着 URGE 实际创建了整段文字纹理。

[GDI CreateFont](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createfontw)说明了字符单元与 em 高度的区别；[GetTextExtentPoint32](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-gettextextentpoint32w)说明测量不考虑裁切；[GetCharABCWidths](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getcharabcwidthsw)与[GetGlyphOutline](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getglyphoutlinew)区分字符推进量、字形原点和像素黑盒，普通字体也允许负边距；[FreeType 字形文档](https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html)明确区分16.16的线性 advance 与受 hinting 影响的26.6 advance。它们约束后端实现，但不能代替 RGSS 行为对照。

回归对照需同时覆盖同一字体文件、字号、粗斜体、测量、逐字笔位置、宽框与紧框、三个对齐方向、描边/阴影、奇偶宽高及 Bitmap 边界；只比较 `text_size` 不能证明绘制兼容。当前对照基于真实 Windows 上的原版 RGSS3 与 GDI，使用相同字体文件，并核验 GDI 实际选中字库的 SHA256；不使用 CrossOver 结果作为 Windows 基准。萝莉体和黑体18–32号、普通/粗体的576组文字测量宽高均已核对，另外覆盖紧框数字、问号、描边和实际帮助窗口的逐字排版。灰度栅格化与压缩采样仍可能存在像素差异，不承诺所有字体逐像素相同。

这仍是在游戏原始分辨率的 Bitmap 中绘制文字。Retina 与窗口缩放继续由现有显示管线处理；本模块没有把整个游戏 Bitmap 改为高分辨率，也不保证任意放大后都如原生桌面 UI 一样平滑。

## 缓存与 GPU 合成

- 字形冷缓存时由 CPU 栅格化为 A8 覆盖率，上传到共享的 R8 图集；颜色、渐变、透明度、阴影和描边图层在 GPU 合成。改变文字颜色不需要重新栅格化字形。
- 每页图集为 1024×1024，最多保留 8 页活动缓存；使用 LRU 淘汰整页。淘汰不覆盖旧纹理，尚未提交或执行中的绘制继续持有旧资源，结束后释放，因此瞬时资源占用可能超过活动缓存预算。
- 每次画布队列提交前，将同页新增字形的脏矩形合并上传，避免每次 `draw_text` 都创建或上传整串 RGBA 文字纹理。重复数字可以共用已有字形；顶点数据仍需上传。
- 排版缓存最多 1024 项、估算 2 MiB；字体面与字形 advance 也有数量限制。图集包含透明边框，避免线性采样串入相邻字形。

设置环境变量 `URGE_FONT_STATS=1`，退出时打印排版命中、栅格化数量、图集上传次数/字节、顶点字节与淘汰次数。上传字节仅统计字体模块主动写入的图集区域，不代表总 GPU 流量。

macOS 使用 CoreText 的文字运行与缺字回退；非 macOS 后端保留常用 CJK、拉丁文、西里尔文字的字形选择与 kerning 范围，没有新增完整的复杂文字 shaping 引擎。

## 系统字体发现与缺字回退

脚本可以指定字体文件名（可省略扩展名）或字体族名。游戏 `Fonts/` 中的同名字体不会被系统文件覆盖。按族名选择时，游戏字体也优先于系统字体，包含游戏内 TTF 被改名的情况；显式指定系统文件名仍按该文件名查找。系统字体只登记路径，需要时才读取完整文件。

| 平台 | 系统字体发现 | 缺字处理 |
| --- | --- | --- |
| Windows | 现有系统与用户 Fonts 目录 | 字体名候选与内嵌字体 |
| macOS | `/System/Library/Fonts`（含 `Supplemental`）、`/Library/Fonts`、`~/Library/Fonts` | 平台默认字体与 CoreText 缺字回退 |
| Linux | `/usr/share/fonts`、`/usr/local/share/fonts`、XDG 字体目录、`~/.fonts` | 可用 Noto CJK、文泉驿等默认字体 |
| Android | `/system/fonts`、`/product/fonts`、`/vendor/fonts` | 可用 Noto CJK 等默认字体 |
| WASM/Web | 游戏 `Fonts/` 与内嵌字体 | 中文字体需随游戏打包并指定 |

Linux 的 XDG 路径包括 `XDG_DATA_DIRS` 下的 `fonts` 和 `XDG_DATA_HOME/fonts`（未设置时使用 `~/.local/share/fonts`）。不解析 Fontconfig 自定义规则，也不保证覆盖 Android 厂商专用路径和动态字体配置。无法访问的目录会跳过；未发现中文字体时不会下载字体，内嵌字体也不保证覆盖中文。

WASM 不枚举浏览器宿主的系统字库。字体栅格化代码与其他非 macOS 平台共用，不依赖浏览器字体枚举或本机字体授权接口。

Bitmap 的字体在创建时取得当前默认设置；修改 `bitmap.font.name` 或 `size` 后，测量和绘制使用对应的新字体面及缓存。`bitmap.font = font` 与 `bitmap.clone` 都复制字体设置，副本可以独立修改字号。Window 的文字通过其 `contents` Bitmap 使用同一流程。

`Window_Base#text_size` 等脚本封装最终调用 `contents.text_size`。Window 的 `padding` 位于窗口边框与 `contents` 之间，只影响内容区域的大小、显示位置和裁剪，不加入文字测量值，也不由 Bitmap 再次扣除；自定义脚本的 `text_width` / `text_rect` 同样取决于其对 `Bitmap#text_size` 的封装。
