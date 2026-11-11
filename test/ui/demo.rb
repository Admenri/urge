# Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
# Double-click test/ui/demo/Game.exe. Refresh: ./test/ui/run.ps1 -Demo -PrepareOnly.
# demo_theme.rb is bundled by pack.rb; see doc/UI.md for the Unity mapping.
Graphics.resize_screen(800, 620)
canvas = UI::Canvas.new(reference_size: Vector2.new(800, 620), scale_mode: UI::SCALE_WITH_SCREEN_SIZE)
themes = %i[blue paper pixel].to_h { |key| [key, UIDemoTheme.new(key)] }
gold = UIDemoTheme.new(:gold)
styled_nodes = []
register = ->(node, role) { styled_nodes << [node, role]; node }
background = register.call(UI::Panel.new(canvas, size: [800, 620], raycast_target: false), :page)

make_label = lambda do |text, x, y, width, height = 28, role = :text, parent = canvas|
  node = UI::Text.new(parent, text: text, size: [width, height])
  node.position = Vector3.new(x, y, 0)
  register.call(node, role)
end
make_button = lambda do |text, x, y, width, height = 38, parent = canvas|
  node = UI::Button.new(parent, text: text, size: [width, height])
  node.position = Vector3.new(x, y, 0)
  register.call(node, :button)
end
make_label.call('URGE / UI 皮肤演示', 24, 14, 744, 40, :title)
make_label.call('统一主题 + 状态图片 + 九宫格 + 单控件覆盖', 24, 98, 744, 26, :caption)
status = make_label.call('', 24, 580, 744, 28, :caption)
skin_buttons = {}
themes.each_with_index do |(key, theme), index|
  skin_buttons[key] = make_button.call(theme.name, 24 + index * 172, 60, 160, 34)
end
make_label.call('点击左侧按钮切换主题', 548, 62, 228, 30, :caption)

left = UI::LayoutGroup.new(canvas, size: [330, 330], layout_mode: UI::VERTICAL)
left.position = Vector3.new(24, 130, 0)
left.padding = Vector4.new(0, 0, 0, 0)
left.spacing = Vector2.new(0, 10)
row = lambda do |type, height, role, options = {}|
  register.call(type.new(left, size: [330, height], **options), role)
end
row.call(UI::Text, 24, :text, text: '交互控件：悬停 / 按下 / 焦点')
open_dialog = row.call(UI::Button, 42, :button, text: '打开主题对话框')
toggle = row.call(UI::Toggle, 38, :toggle, text: '开启背景音乐', checked: true)
toggle.on_value_changed { status.text = "背景音乐：#{toggle.checked ? '开启' : '关闭'}" }
slider = row.call(UI::Slider, 38, :control, max_value: 100.0, step: 1.0, value: 65.0)
progress = row.call(UI::ProgressBar, 18, :control, max_value: 100.0, value: 65.0)
slider.on_value_changed { progress.value = slider.value; status.text = "音量：#{slider.value.round}%" }
input = row.call(UI::InputField, 38, :input, placeholder: '输入名字，支持中文输入法', character_limit: 24)
input.on_submit { status.text = "你好，#{input.text}！" }
row.call(UI::InputField, 60, :input, multiline: true, placeholder: "多行备注\n换肤不会丢失输入内容")
dropdown = register.call(UI::Dropdown.new(canvas, text: '选择画面质量', size: [330, 38],
  items: ['流畅', '均衡', '高清'], row_height: 28.0), :button)
dropdown.position = Vector3.new(24, 472, 0)
dropdown.z = 100
dropdown.on_selection_changed { status.text = "画面质量：#{dropdown.items[dropdown.selected_index]}" }
make_label.call('主题修改外观，控件状态保持不变。', 24, 530, 330, 28, :caption)

make_label.call('同一主题应用到滚动区和列表', 388, 130, 388, 24)
scroll = register.call(UI::ScrollPanel.new(canvas, size: [350, 128], layout_mode: UI::VERTICAL), :panel)
scroll.position = Vector3.new(388, 164, 0)
scroll.padding = Vector4.new(8, 8, 8, 8)
scroll.spacing = Vector2.new(0, 8)
12.times do |i|
  item = register.call(UI::Button.new(scroll, text: "背包物品 #{i + 1}", size: [330, 36]), :button)
  item.on_click { status.text = "选中了背包物品 #{i + 1}" }
end
bar = register.call(UI::ScrollBar.new(canvas, size: [22, 128]), :control)
bar.position = Vector3.new(746, 164, 0)
bar.bind_scroll(scroll)
list = register.call(UI::List.new(canvas, size: [380, 100],
  items: Array.new(30) { |i| "任务 #{i + 1}" }), :panel)
list.position = Vector3.new(388, 308, 0)
list.on_selection_changed { status.text = "选中了#{list.items[list.selected_index]}" }
list.on_submit { status.text = "确认了#{list.items[list.selected_index]}" if list.selected_index >= 0 }
make_label.call('九宫格：边角保持尺寸，中间拉伸', 388, 420, 388, 24, :caption)
small = make_button.call('小按钮', 388, 454, 84)
large = make_button.call('同一张皮肤的宽按钮', 482, 454, 286)
small.on_click { status.text = '84 px 和 286 px 按钮共用一张 48 px 的九宫格图片。' }
large.on_click { status.text = '九宫格只拉伸中心和边缘，角部仍是 8 px。' }
special = UI::Button.new(canvas, text: '独立金色皮肤', size: [228, 40])
special.position = Vector3.new(398, 520, 0)
special.quaternion = Vector4.new(0, 0, Math.sin(-0.035), Math.cos(-0.035))
special.style = gold.style(:button)
special.on_click { status.text = '这个控件覆盖了主题样式，切换全局主题后仍保持金色。' }
disabled = make_button.call('禁用状态', 642, 514, 126, 40)
disabled.interactable = false
make_label.call('金色按钮单独覆盖；右侧使用禁用贴图。', 388, 552, 388, 24, :caption)

dialog = register.call(UI::Dialog.new(canvas, size: [360, 180], anchor: :center, visible: false), :panel)
dialog.z = 1000
make_label.call("模态对话框也会同步换肤\n背景颜色、字体、按钮资源一起更换。", 16, 16, 328, 78, :text, dialog)
close = make_button.call('关闭', 100, 120, 160, 42, dialog)
close.on_click { dialog.visible = false; open_dialog.focus }
dialog.on_cancel { dialog.visible = false; open_dialog.focus }
open_dialog.on_click { dialog.visible = true; close.focus }

apply_theme = lambda do |key|
  theme = themes.fetch(key)
  styled_nodes.each { |node, role| node.style = theme.style(role) }
  skin_buttons.each { |button_key, button| button.text = "#{button_key == key ? '● ' : ''}#{themes[button_key].name}" }
  status.text = "当前皮肤：#{theme.name}；悬停、按下按钮可观察状态图片。"
end
skin_buttons.each { |key, button| button.on_click { apply_theme.call(key) } }
apply_theme.call(:blue)

begin
  loop { Graphics.update }
ensure
  canvas.dispose
  themes.each_value(&:dispose)
  gold.dispose
end
