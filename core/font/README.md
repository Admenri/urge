# 字体发现与渲染

游戏脚本继续使用 `Font`、`Bitmap#text_size` 和 `Bitmap#draw_text`。字体后端、排版缓存、字形图集和字体着色器均位于 `core/font`，不引入 Skia；借鉴其“排版与栅格化分离、覆盖率字形缓存、GPU 合成”的设计。画布只接收通用的三角形批次和提交前准备回调。

## 抗锯齿与尺寸

| 平台 | 字体后端 |
| --- | --- |
| macOS | CoreText 排版，CoreGraphics 灰度覆盖率栅格化 |
| Windows / Linux / Android / WASM | 仓库内 FreeType，轮廓优先与灰度栅格化 |

普通文字优先使用字体轮廓，跳过 SimHei 等字体中常见的内嵌单色位图，避免这些位图使灰度抗锯齿失效。描边同样由轮廓生成；透明文字使用单通道覆盖率，不使用会在彩色背景上产生色边的 LCD 子像素抗锯齿。`Font.solid` 仍显式选择单色输出。只有位图、没有轮廓的字体保留位图回退。

RGSS1/2/3 的默认字号分别为 22/20/24，单位为游戏逻辑像素的**字符单元高度**。后端根据字体的 Windows ascent/descent（OpenType `OS/2` 表中的 `usWinAscent` / `usWinDescent`）换算 em 尺寸和基线；这些度量不可用时使用后端提供的 ascent/descent。字体的 line gap 不计入 RGSS 行框。

因此不再把 `font.size` 直接当作 OpenType 的 em 大小，也不使用固定 0.9 系数。SimHei 的单元高度恰好为 1 em，尺寸保持原样；Lolita 等单元高度大于 1 em 的字体按自身比例换算，避免字号 24 产生约 33 像素行高而被 24 像素绘制框截断。缺字回退字体也采用同一单元高度，分别定位其基线，macOS 的回退运行重新排版以保持测量和实际字形间距一致。

字体布局与测量共用同一后端，`font.size` 属性和脚本行高不因这个换算而改变；描边、阴影不改变 `text_size` 的文字度量。这个尺寸契约与 [GDI 正字体高度的字符单元定义](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createfontw) 一致。

### `Bitmap#draw_text` 的 RGSS 缩放行为

RGSS1/2/3 手册均规定：文字超出传入矩形的宽度时自动横向压缩，最小为原宽的 60%。两个重载 `draw_text(x, y, width, height, str, align)` 与 `draw_text(rect, str, align)` 使用相同行为。

- 按 `text_size` 的文字逻辑宽度与传入矩形的 `width` 比较；不按与 Bitmap 相交后剩余的可见宽度计算。描边和阴影不改变文字的逻辑度量。
- 压缩比例为 `max(0.6, min(1.0, width / text_width))`，只压缩横向；不放大短文字，不改变 `font.size` 或 `text_size` 的返回值，也不重排换行。
- 先按压缩后的文字宽度执行左/中/右对齐，居中的偏移取整；达到下限后仍超宽时从框的起点绘制并裁剪，避免负偏移截掉文字开头。
- 垂直保持原始字号，空间足够时居中；高度不足时从框的顶部开始并裁剪，不为了适应高度而缩小字形。最终绘制受传入矩形和 Bitmap 边界共同裁剪。

文档来源：[RGSS1 / XP](https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_bitmap.html)、[RGSS2 / VX](https://www.rpg-maker.fr/dl/monos/aide/vx/source/rgss/gc_bitmap.html)、[RGSS3 / VX Ace 手册转载](https://pdfcoffee.com/rpg-maker-vx-ace-help-rgss3-reference-manual-4-pdf-free.html)。兼容以文档定义的缩放与布局语义为准。原版 GDI 会按整数字宽重新选字形，具体字体的最终字形宽度和像素结果可能与这里的连续几何压缩不同。

这仍是在游戏原始分辨率的 Bitmap 中绘制文字。Retina 与窗口缩放继续由现有显示管线处理；本模块没有把整个游戏 Bitmap 改为高分辨率，也不保证任意放大后都如原生桌面 UI 一样平滑。

## 缓存与 GPU 合成

- 字形冷缓存时由 CPU 栅格化为 A8 覆盖率，上传到共享的 R8 图集；颜色、渐变、透明度、阴影和描边图层在 GPU 合成。改变文字颜色不需要重新栅格化字形。
- 每页图集为 1024×1024，最多保留 8 页活动缓存；使用 LRU 淘汰整页。淘汰不覆盖旧纹理，尚未提交或执行中的绘制继续持有旧资源，结束后释放，因此瞬时资源占用可能超过活动缓存预算。
- 每次画布队列提交前，将同页新增字形的脏矩形合并上传，避免每次 `draw_text` 都创建或上传整串 RGBA 文字纹理。重复数字可以共用已有字形；顶点数据仍需上传。
- 排版缓存最多 1024 项、估算 2 MiB；字体面与字形 advance 也有数量限制。图集包含透明边框，避免线性采样串入相邻字形。

设置环境变量 `URGE_FONT_STATS=1`，退出时打印排版命中、栅格化数量、图集上传次数/字节、顶点字节与淘汰次数。上传字节仅统计字体模块主动写入的图集区域，不代表总 GPU 流量。

macOS 使用 CoreText 的文字运行与缺字回退；非 macOS 后端保留常用 CJK、拉丁文、西里尔文字的字形选择与 kerning 范围，没有新增完整的复杂文字 shaping 引擎。

## 系统字体发现与缺字回退

脚本可以指定字体文件名（可省略扩展名）或字体族名。游戏 `Fonts/` 中的同名字体不会被系统文件覆盖。系统字体只登记路径，需要时才读取完整文件。

| 平台 | 系统字体发现 | 缺字处理 |
| --- | --- | --- |
| Windows | 现有系统与用户 Fonts 目录 | 字体名候选与内嵌字体 |
| macOS | `/System/Library/Fonts`（含 `Supplemental`）、`/Library/Fonts`、`~/Library/Fonts` | 平台默认字体与 CoreText 缺字回退 |
| Linux | `/usr/share/fonts`、`/usr/local/share/fonts`、XDG 字体目录、`~/.fonts` | 可用 Noto CJK、文泉驿等默认字体 |
| Android | `/system/fonts`、`/product/fonts`、`/vendor/fonts` | 可用 Noto CJK 等默认字体 |
| WASM/Web | 游戏 `Fonts/` 与内嵌字体 | 中文字体需随游戏打包并指定 |

Linux 的 XDG 路径包括 `XDG_DATA_DIRS` 下的 `fonts` 和 `XDG_DATA_HOME/fonts`（未设置时使用 `~/.local/share/fonts`）。不解析 Fontconfig 自定义规则，也不保证覆盖 Android 厂商专用路径和动态字体配置。无法访问的目录会跳过；未发现中文字体时不会下载字体，内嵌字体也不保证覆盖中文。

WASM 不枚举浏览器宿主的系统字库。字体栅格化代码与其他非 macOS 平台共用，不依赖浏览器字体枚举或本机字体授权接口。
