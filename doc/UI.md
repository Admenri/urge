# Node UI

游戏内 retained UI，实现位于 `core/ui`，Ruby 便利接口位于 `bind/ui_api.rb`。
演示和回归脚本位于 `test/ui`。所有控件直接继承 `Node`，与
`Sprite`、`Plane` 平级，共用 Parent、Z、Position、Quaternion、Scale、Camera。
控件通过组合 `UIState` 共享布局、外观与交互；原生层不依赖 Ruby。

## 快速使用

```ruby
canvas = UI::Canvas.new
button = UI::Button.new(canvas, text: '开始游戏', size: [200, 48], anchor: :center)
button.on_click { |event| puts event.target.text }
input = UI::InputField.new(canvas, placeholder: '输入名字', character_limit: 24)
input.position = Vector3.new(30, 30, 0)
input.on_submit { puts input.text }
loop { Graphics.update }
```

顶层名称 `UIButton` 等与 `UI::Button` 是同一个类。Ruby 自动加载便利层，
无需 require。C++ 用法是 `MakeRefCounted<UIButton>(parent)`，
通过 `State()` 和 `RectTransform()` 配置，`UI::Get().PollEvent()` 取事件。
Ruby 父控件持有子控件 wrapper；`dispose` 递归销毁 UI 子控件并清理回调。
插入普通 Node/Sprite/Plane 父节点时，保留该节点的 Ruby 引用，并管理其生命周期。

## 控件

| 类 | 用途与主要属性 |
| --- | --- |
| UICanvas | UI 根节点、参考分辨率、自适应缩放、Raycaster |
| UIPanel | 容器、背景、透明度、交互分组与遮罩 |
| UIImage | Bitmap，stretch / contain / nine-slice |
| UIText | 字体、颜色、多行、自动折行、水平/垂直对齐 |
| UIButton | hover / pressed / disabled / focused，click |
| UIToggle | checked，value_changed |
| UISlider | min_value / max_value / step / value，横向或纵向 |
| UIProgressBar | 进度，横向或纵向 |
| UIScrollPanel | 滚轮、触摸拖动、content_size 自动测量、clip_children |
| UIInputField | UTF-8、IME、剪贴板、选择、密码、数字、只读、字符限制、多行 |
| UILayoutGroup | horizontal / vertical / grid，padding、spacing、flex |
| UIDropdown | items、selected_index、展开选择、滚轮、键盘 |
| UIList | 字符串列表、选择、滚动，只绘制可见行 |
| UIDialog | modal，背景输入阻挡、焦点约束、cancel |
| UIScrollBar | 与 ScrollPanel 通过 bind_scroll 关联 |
| UISeparator | 分隔线 |

## RectTransform 与 Canvas

布局沿用 Unity 的 anchor、pivot 和 size delta 概念；屏幕左上为原点，Y 向下。
`Node.position` 是布局位置之外的偏移，旋转和缩放作用于 pivot。

```ruby
button.rect_transform.anchor_min = Vector2.new(0.5, 0.5)
button.rect_transform.anchor_max = Vector2.new(0.5, 0.5)
button.rect_transform.pivot = Vector2.new(0.5, 0.5)
button.anchor = :center # 上面三行的快捷写法
panel.anchor = :stretch
panel.width = -40 # stretch 后的 size delta
panel.height = -40
panel.position = Vector3.new(20, 20, 0)
```

支持九个 anchor 预设和 `:stretch`，min_size / max_size、expand_width /
expand_height、ignore_layout。`rect_transform.resolved_size` 读取布局后的尺寸。
布局组接受 UI 子节点；隐藏节点不参与。布局组驱动尺寸/位置，显式 position
仍作为额外偏移。ScrollPanel 同样可设置 layout_mode，content_size 的零分量自动测量。

Canvas 默认使用当前逻辑屏幕尺寸。`reference_size` 与 `scale_mode` 可配置：
`CONSTANT_PIXEL_SIZE`、`FIT`、`SCALE_WITH_SCREEN_SIZE`。
最后一种通过 `match_width_or_height` 在宽、高缩放比间对数插值。
将 Camera/PerspectiveCamera 设置在 Canvas 或祖先 Node 上即可使用相机空间 UI。
设置 3D position/quaternion 可创建世界平面 UI；Canvas 仍属于 Node 场景树。

## 外观

每个控件默认独立 UIStyle，也可显式共享 `widget.style = theme`。
背景、前景、hover、pressed、disabled、accent、边框颜色均可配置；
`style.font` 使用现有 Font。`style.skin` + `style.slice` 提供九宫格背景。
`image.image_mode = UI::CONTAIN` 保持比例，`UI::NINE_SLICE` 使用相同 slice。
`opacity` 在祖先链上相乘。文本 alignment：0 左 / 1 中 / 2 右；
vertical_alignment：0 上 / 1 中 / 2 下。

### 与 Unity 的皮肤配置对应

这里主要参考 Unity 的 uGUI。控件的行为组件通过子节点的 Image、Text/TMP 等
Graphic 定义外观；常用配置如下：

| Unity uGUI 组件 | 自定义外观的方式 | 本引擎对应 |
| --- | --- | --- |
| Image / RawImage | Sprite/Texture、Color、Material；Image 的 Sliced 类型和 Sprite Border 实现九宫格 | UIImage.bitmap、image_mode、UIStyle.slice |
| Text / TextMeshPro | 字体或 Font Asset、字号、颜色；TMP 的 Material Preset 控制描边等效果 | UIStyle.font、foreground |
| Button / Selectable | Target Graphic；Transition 可选 Color Tint、Sprite Swap、Animation，分别配置交互状态 | 状态颜色、skin / hover_skin / pressed_skin / disabled_skin / focused_skin |
| Toggle / Slider / Scrollbar | 指定 Background、Checkmark、Fill、Handle 等子 Graphic 的资源 | UIStyle 的背景、accent、前景和状态皮肤 |
| InputField / Dropdown | 背景的 Selectable 状态，加上文本、placeholder、展开模板和条目的 Graphic | UIStyle、placeholder、items 与选择状态 |
| ScrollRect / Panel | Viewport、Content、背景 Image 和 Scrollbar 的组合 | UIScrollPanel、UIPanel、UIScrollBar |

统一复用通常使用 Prefab / Prefab Variant；运行时换肤可以用自建
ScriptableObject Theme 保存 Sprite、颜色、字体，并分配给组件。
Unity UI Toolkit 使用 UXML + USS 样式类和伪类，属于另一套 UI 工作流。

本引擎可共享 UIStyle，实现同类控件同步换肤；单个控件赋予独立 UIStyle
即可覆盖主题。普通填充背景使用状态颜色，图片皮肤使用状态图片切换：

```ruby
button_style = UI::Style.new
button_style.skin = Bitmap.new('Skins/blue_button.png')
button_style.hover_skin = Bitmap.new('Skins/blue_hover.png')
button_style.pressed_skin = Bitmap.new('Skins/blue_pressed.png')
button_style.disabled_skin = Bitmap.new('Skins/blue_disabled.png')
button_style.focused_skin = Bitmap.new('Skins/blue_focused.png')
button_style.slice = Vector4.new(8, 8, 8, 8)
button_style.border_width = 0
button_style.font.size = 19
button.style = button_style
```

状态优先级为禁用、按下、悬停、焦点、普通；缺少状态图片时回退到普通 skin。
图片中的透明区域保持透明。`test/ui/demo_theme.rb` 提供三套可切换主题，
统一设置字体、颜色和图片；demo 中金色按钮使用独立样式。
首次运行会在临时目录 `test/ui/demo/Skins` 生成 48×48 PNG；可以替换这些 PNG
再重新打开 demo，尝试自己的美术资源。四边的 slice 默认为 8 px。

## 射线检测与遮罩

```ruby
hit = canvas.raycaster.raycast_screen(mouse_x, mouse_y)
hit = canvas.raycaster.raycast(Vector3.new(0, 0, 10), Vector3.new(0, 0, -1), 100.0)
puts [hit.target, hit.local_position, hit.uv, hit.distance] if hit
canvas.raycaster.sort_by_distance = true
```

屏幕检测通过相机逆投影生成射线，转入控件局部坐标后与 Z=0 平面求交。
逆变换后的方向保留参数长度，因此非均匀缩放不改变世界距离比较。
支持父级 TRS、镜像、旋转、透视、正交；退化矩阵、平行射线、距离范围之外
不命中。默认按真实 Node 绘制顺序选择最上层；可改为距离优先。

`raycast_target=false` 忽略本控件，保留子节点；`blocks_raycasts=false`
忽略整个子树；`interactable=false` 阻止交互但仍可遮挡后方 UI。
`double_sided` 控制背面命中。`clip_children` 对渲染和检测使用同一投影遮罩；
旋转遮罩采用多边形裁剪，不能用屏幕 AABB 代替。支持嵌套遮罩和近裁剪面穿越。
Viewport 的已有屏幕 scissor 同样过滤 UI 射线。

## 事件与输入

`on(type)` / `off(type, handler)`，以及 `on_click`、`on_value_changed`、
`on_selection_changed`、`on_submit`、`on_cancel`、`on_focus`、`on_blur`、
`on_pointer_enter/exit/down/up/move/cancel`、`on_drag`、`on_scroll`。
事件包含 type、target、current_target、pointer_id、position、local_position，
`stop_propagation` 终止冒泡。local_position 始终相对于初始 target。
回调在 `Graphics.update` 返回后执行，允许创建/销毁控件；手动注入事件后调用
`UI.dispatch`，或用 `UI.poll_event` 自行消费原生事件。

鼠标、触摸使用 pointer capture；松开点离开原控件或触发滚动拖动时不 click。
Tab / Shift+Tab 导航，方向键调整数值或列表，Enter / Space 激活，Escape cancel。
列表 Enter / Space 确认当前选中项并触发 `on_submit`；非文本控件也支持小键盘
2/4/6/8 导航。输入框在 Num Lock 开启时保留小键盘数字输入。
手柄 D-pad / South / East 使用相同导航。`widget.focus` 设置焦点。
输入框阻挡游戏键盘输入，模态对话框将交互和导航限制到其子树。

输入框支持鼠标拖选、Ctrl+A/C/V/X、Shift+左右/Home/End/上下选择、Backspace/Delete，
多行 Enter 换行、Ctrl+Enter submit、单行 Enter submit。字符限制按 Unicode
码点计数；cursor_position / selection_position 是 UTF-8 字节偏移。
`style.selection_color` 独立控制文字选择高亮，避免与悬停皮肤混在一起。

`scrollbar.bind_scroll(panel, :vertical)` 将滚动条绑定到 ScrollPanel 或 List；
`:horizontal` 使用横向。滑块长度按可见范围与内容大小计算，布局调整或直接
修改 `panel.scroll` 时自动同步；抓住滑块边缘拖动不会跳到中心。
独立滚动条可设置 `thumb_size`（0..1），绑定后由内容比例自动管理。

## 实现边界

布局与命中以矩形平面为基础；当前无圆角/非矩形 alpha 命中、富文本、
矢量路径、文本撤销栈、完整 Unicode grapheme 导航或无障碍桥接。
自动折行按码点测量，不提供复杂段落排版。列表提供字符串行，不含任意
控件模板的节点虚拟化。Dropdown 弹出区属于原节点，受祖先遮罩和 Z 顺序约束；
可通过较高 z 显示在同父级控件之上。ScrollPanel 不含惯性和弹性越界。
控件内容缓存为 Bitmap，变更时重新绘制；稳定内容复用缓存并使用 SpriteBatch。
大型动态文本/大量独立控件应测量显存和重绘成本。

## 示例和验证

`test/ui/demo.rb` 是可运行控件展示，`test/ui/regression.rb` 是引擎内交互/渲染回归。
Windows 在仓库根目录执行 `./test/ui/run.ps1 -Demo` 可直接打包并打开演示窗口。
运行文件放在 `test/ui/demo`，可直接双击 `test/ui/demo/Game.exe`。
该目录作为临时文件整体被 Git 忽略；重新编译引擎或修改 demo 后，执行
`./test/ui/run.ps1 -Demo -PrepareOnly` 可更新 exe、配置和打包脚本而不启动窗口。
手动建立游戏目录时，放置 Game.exe 和配置：

```ini
[Game]
Title=URGE UI
Scripts=Data/Scripts.rxdata
RGSS=1
[GFX]
Backend=opengl
SpriteBatch=true
PresentSampler=linear
[Display]
Width=900
Height=640
VSync=false
```

用 Ruby + zlib 将脚本打包为 `Marshal.dump([[1, 'Main', Zlib::Deflate.deflate(code)]])`
写入 Data/Scripts.rxdata。运行 Game.exe；回归脚本会生成 regression.txt、
controls.png、rotated-mask.png 后退出。详见 `test/ui/run.ps1`。

演示使用 `PresentSampler=linear`。Windows 在 125% / 150% 等非整数显示缩放下，
nearest 会使中文细笔画宽度不均，窗口截图也可能出现笔画缺失；线性采样适合
此类文字界面。像素风游戏可自行选择 nearest；控件原始 Bitmap 不受显示采样影响。

数学测试独立编译：

```sh
cmake -S test/ui -B build/ui-math-tests
cmake --build build/ui-math-tests --config Release
ctest --test-dir build/ui-math-tests -C Release --output-on-failure
```

Windows 可在 configure 添加 `-G "Visual Studio 18 2026" -A x64`。
