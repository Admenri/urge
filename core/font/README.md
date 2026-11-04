# 平台字体发现与缺字回退

字体仍由 SDL_ttf/FreeType 渲染。游戏脚本可以使用字体文件名（可省略扩展名）或字体族名。

| 平台 | 系统字体发现 | 缺字处理 |
| --- | --- | --- |
| Windows | 保持现有 Windows 系统与用户 Fonts 目录扫描 | 保持现有选择逻辑 |
| macOS | `/System/Library/Fonts`、其 `Supplemental` 目录、`/Library/Fonts`、`~/Library/Fonts` | 优先选择可用的系统中文默认字体，为选中字体补充缺少的字形 |
| Linux | 递归扫描 `/usr/share/fonts`、`/usr/local/share/fonts`、`XDG_DATA_DIRS` 下的 `fonts`、`XDG_DATA_HOME/fonts`（未设置时使用 `~/.local/share/fonts`）、`~/.fonts` | 优先选择可用 Noto CJK、文泉驿等中文字体并提供缺字回退 |
| Android | `/system/fonts`、`/product/fonts`、`/vendor/fonts` | 优先选择可用 Noto CJK 等中文字体并提供缺字回退 |
| WASM/Web | 保持游戏 `Fonts/` 与内嵌字体；不自动读取宿主系统字库 | 保持现状；中文字体需随游戏打包，并通过 `Font.default_name` 或 `Font.name` 指定 |

游戏 `Fonts/` 中的同名字体文件不会被系统文件覆盖。系统字体只登记路径，需要渲染时才读取完整文件；字体对象按文件、字号、样式与描边缓存。平台默认回退字体使用相同字号、样式和描边，且不回退到自身。

这也覆盖了字体名数组的一个常见情况：游戏指定的“黑体、SimHei”等字体不存在，但较后的 Tahoma/Arial 存在。选到的西文字体保留其原有字形，缺少的汉字再交给平台中文默认字体。未发现中文字体时不会自动下载字体；字体不可读或损坏时仍可使用内嵌字体，避免全部文字消失。内嵌字体本身不保证覆盖中文。

此实现按常用目录、文件名及字体集合首个字体面的族名进行发现，不等同于完整的操作系统字体匹配服务：Linux 不解析 Fontconfig 的自定义规则，Android 不保证覆盖厂商专用路径与所有动态字体配置。无法访问的目录会跳过，不要求安装额外字体枚举库，也没有提高 Android API 级别。

浏览器原始本机字体访问需要专门授权，无法作为透明、通用的自动回退，因此 Web 维持原路径；参见 [Local Font Access API](https://developer.chrome.com/docs/capabilities/web-apis/local-fonts)。
