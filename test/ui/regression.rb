# Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
# Pack this script as Data/Scripts.rxdata and run with Game.exe (see doc/UI.md).
results = []
def assert(value, message = 'assertion failed')
  raise message unless value
end
def near(a, b)
  (a - b).abs < 0.02
end
def point(node, x, y)
  m = node.world_matrix
  [m[0] * x + m[4] * y + m[12], m[1] * x + m[5] * y + m[13]]
end
def click(x, y)
  UI.pointer('down', x, y)
  UI.pointer('up', x, y)
  UI.dispatch
end
def check(results, name)
  canvas = UI::Canvas.new
  yield canvas
  results << "PASS #{name}"
rescue Exception => error
  results << "FAIL #{name}: #{error.class}: #{error.message}\n#{error.backtrace&.first(3)&.join("\n")}"
ensure
  canvas&.dispose
  UI.dispatch
end

Graphics.resize_screen(800, 600)
check(results, 'all 16 widgets are Node subclasses and render') do |canvas|
  types = [UICanvas, UIPanel, UIImage, UIText, UIButton, UIToggle, UISlider,
    UIProgressBar, UIScrollPanel, UIInputField, UILayoutGroup, UIDropdown,
    UIList, UIDialog, UIScrollBar, UISeparator]
  types.each_with_index do |type, i|
    assert(type.superclass == Node, "#{type} superclass")
    next if type == UICanvas
    widget = type.new(canvas)
    widget.position = Vector3.new(10 + (i % 4) * 190, 10 + (i / 4) * 120, 0)
    widget.size = [170, 90]
    widget.text = type.name
    widget.modal = false
  end
  Graphics.update
  image = Graphics.snap_to_bitmap
  image.to_image.save_file('controls.png')
  assert(image.get_pixel(20, 150).red > 10, 'rendered widget pixels')
end
check(results, 'center and stretched RectTransform') do |canvas|
  button = UI::Button.new(canvas, text: 'Center', size: [100, 40], anchor: :center)
  panel = UI::Panel.new(canvas)
  panel.anchor = :stretch
  panel.width = -20
  panel.height = -30
  panel.raycast_target = false
  UI.update
  assert(near(button.world_matrix[12], 400) && near(button.world_matrix[13], 300))
  assert(near(panel.rect_transform.resolved_size.x, 780))
  assert(near(panel.rect_transform.resolved_size.y, 570))
end
check(results, 'horizontal vertical grid and flex layouts') do |canvas|
  group = UI::LayoutGroup.new(canvas, size: [300, 100], layout_mode: UI::HORIZONTAL)
  group.padding = Vector4.new(10, 10, 10, 10)
  group.spacing = Vector2.new(5, 5)
  a = UI::Button.new(group, size: [50, 30])
  b = UI::Button.new(group, size: [20, 30])
  b.rect_transform.expand_width = true
  UI.update
  assert(near(a.world_matrix[12], 10))
  assert(near(b.world_matrix[12], 65))
  assert(near(b.rect_transform.resolved_size.x, 225))
  group.layout_mode = UI::VERTICAL
  UI.update
  assert(near(b.world_matrix[13], 45))
  group.layout_mode = UI::GRID
  group.columns = 2
  group.cell_size = Vector2.new(80, 30)
  c = UI::Button.new(group)
  UI.update
  assert(near(c.world_matrix[12], 10) && near(c.world_matrix[13], 45))
end
check(results, 'canvas reference scaling') do |canvas|
  canvas.reference_size = Vector2.new(400, 300)
  canvas.scale_mode = UI::FIT
  button = UI::Button.new(canvas, anchor: :center)
  UI.update
  assert(near(button.world_matrix[0], 2))
  assert(near(button.world_matrix[12], 400))
end
check(results, 'raycaster parent transform mirror and mutable vectors') do |canvas|
  parent = Node.new(canvas)
  parent.position = Vector3.new(280, 180, 0)
  parent.scale = Vector3.new(-1.5, 0.6, 1)
  angle = Math::PI / 5
  parent.quaternion = Vector4.new(0, 0, Math.sin(angle / 2), Math.cos(angle / 2))
  button = UI::Button.new(parent, size: [100, 50])
  UI.update
  x, y = point(button, 40, 20)
  hit = canvas.raycaster.raycast_screen(x, y)
  assert(hit && hit.target.equal?(button), "transformed target identity: #{hit&.target} at #{x},#{y}; expected #{button.state.id}, got #{hit&.target_id}")
  assert(near(hit.local_position.x, 40) && near(hit.local_position.y, 20))
  parent.position.x += 30
  x, y = point(button, 40, 20)
  assert(canvas.raycaster.raycast_screen(x, y)&.target.equal?(button), 'mutable transform')
  x, y = point(button, -5, 20)
  assert(canvas.raycaster.raycast_screen(x, y).nil?, 'outside local rectangle')
  parent.scale.x = 0
  assert(canvas.raycaster.raycast_screen(310, 180).nil?, 'singular transform')
  parent.dispose
end
check(results, 'rotated scroll mask clips hit and geometry') do |canvas|
  panel = UI::ScrollPanel.new(canvas, size: [100, 80])
  panel.position = Vector3.new(300, 230, 0)
  angle = Math::PI / 6
  panel.quaternion = Vector4.new(0, 0, Math.sin(angle / 2), Math.cos(angle / 2))
  button = UI::Button.new(panel, size: [200, 60])
  UI.update
  inside = point(button, 30, 30)
  outside = point(button, 150, 30)
  assert(canvas.raycaster.raycast_screen(*inside)&.target.equal?(button))
  assert(canvas.raycaster.raycast_screen(*outside).nil?, 'mask rejects outside child')
  Graphics.update
  image = Graphics.snap_to_bitmap
  image.to_image.save_file('rotated-mask.png')
  color = image.get_pixel(*outside.map(&:round))
  assert(color.red < 5 && color.green < 5 && color.blue < 5, 'clipped pixels')
end
check(results, 'perspective camera and world ray distances') do |canvas|
  canvas.camera = PerspectiveCamera.new
  button = UI::Button.new(canvas, size: [2, 1])
  button.rect_transform.pivot = Vector2.new(0.5, 0.5)
  button.position = Vector3.new(0, 0, -5)
  button.quaternion = Vector4.new(0, Math.sin(0.25), 0, Math.cos(0.25))
  UI.update
  hit = canvas.raycaster.raycast_screen(400, 300)
  assert(hit && hit.target.equal?(button), 'perspective center hit')
  assert(near(hit.local_position.x, 0) && near(hit.local_position.y, 0))
  hit = canvas.raycaster.raycast(Vector3.new(0, 0, 0), Vector3.new(0, 0, -1), 10.0)
  assert(hit && near(hit.distance, 5), 'world ray distance')
  assert(canvas.raycaster.raycast(Vector3.new(0, 0, 0), Vector3.new(0, 0, -1), 4.0).nil?)
end
check(results, 'clipped polygons preserve later SpriteBatch parameters') do |canvas|
  panel = UI::ScrollPanel.new(canvas, size: [100, 80])
  panel.position = Vector3.new(80, 100, 0)
  button = UI::Button.new(panel, size: [120, 70])
  button.position = Vector3.new(0, 20, 0)
  button.quaternion = Vector4.new(0, 0, Math.sin(0.25), Math.cos(0.25))
  after = UI::Panel.new(canvas, size: [50, 50])
  after.position = Vector3.new(320, 220, 0)
  after.style.background = Color.new(250, 30, 40)
  bitmap = Bitmap.new(20, 20)
  bitmap.fill_rect(0, 0, 20, 20, Color.new(30, 230, 50))
  sprite = Sprite.new
  sprite.parent = canvas
  sprite.bitmap = bitmap
  sprite.position = Vector3.new(420, 220, 0)
  Graphics.update
  image = Graphics.snap_to_bitmap
  red = image.get_pixel(340, 240)
  green = image.get_pixel(430, 230)
  assert(red.red > 220 && red.green < 60, 'later UI uses its own model')
  assert(green.green > 200 && green.red < 60, 'later Sprite uses its own model')
  sprite.dispose
  bitmap.dispose
end
check(results, 'subtree raycasters and raycast groups') do |canvas|
  panel = UI::Panel.new(canvas, size: [240, 120], anchor: :center, raycast_target: false)
  button = UI::Button.new(panel, size: [100, 40], anchor: :center)
  UI.update
  caster = UIRaycaster.new(panel)
  x, y = point(button, 0, 0)
  assert(caster.raycast_screen(x, y)&.target.equal?(button), 'subtree anchor remains in parent layout')
  panel.blocks_raycasts = false
  assert(canvas.raycaster.raycast_screen(x, y).nil?, 'group blocks raycast subtree')
  panel.blocks_raycasts = true
  button.raycast_target = false
  assert(canvas.raycaster.raycast_screen(x, y).nil?, 'graphic ignores raycast')
end
check(results, 'click identity bubbling and stop propagation') do |canvas|
  button = UI::Button.new(canvas, text: 'Click')
  count = 0
  parent_count = 0
  button.on_click { |event| assert(event.target.equal?(button)); count += 1 }
  canvas.on_click { |event| assert(event.current_target.equal?(canvas)); parent_count += 1 }
  click(20, 20)
  assert(count == 1 && parent_count == 1)
  button.on_click(&:stop_propagation)
  click(20, 20)
  assert(count == 2 && parent_count == 1)
  UI.pointer('down', 20, 20)
  UI.pointer('up', 500, 500)
  UI.dispatch
  assert(count == 2, 'release outside cancels click')
end
check(results, 'scroll wheel drag cancels child activation') do |canvas|
  panel = UI::ScrollPanel.new(canvas, size: [200, 100])
  panel.content_size = Vector2.new(200, 500)
  button = UI::Button.new(panel)
  count = 0
  button.on_click { count += 1 }
  UI.pointer('down', 20, 30)
  UI.pointer('move', 20, 10)
  UI.pointer('up', 20, 10)
  UI.dispatch
  assert(count == 0 && panel.scroll.y > 0)
  before = panel.scroll.y
  UI.scroll(20, 20, 0, -1)
  assert(panel.scroll.y > before)
  panel.interactable = false
  before = panel.scroll.y
  UI.scroll(20, 20, 0, -1)
  assert(near(before, panel.scroll.y), 'disabled scrolling')
end
check(results, 'slider toggle list dropdown and navigation') do |canvas|
  slider = UI::Slider.new(canvas, min_value: 10.0, max_value: 20.0, step: 2.0)
  click(152, 20)
  assert(near(slider.value, 20))
  toggle = UI::Toggle.new(canvas)
  toggle.position.y = 60
  click(20, 80)
  assert(toggle.checked)
  UI.key(44) # space
  UI.dispatch
  assert(!toggle.checked)
  list = UI::List.new(canvas, items: ['A', 'B', 'C'])
  list.position.y = 120
  click(20, 170)
  assert(list.selected_index == 1)
  dropdown = UI::Dropdown.new(canvas, items: ['low', 'high'])
  dropdown.position = Vector3.new(350, 10, 0)
  click(370, 30)
  assert(dropdown.expanded)
  click(370, 100)
  assert(dropdown.selected_index == 1 && !dropdown.expanded)
  click(370, 30)
  click(700, 500)
  assert(!dropdown.expanded, 'click outside closes popup')
  UI.focus(slider)
  UI.key(43) # tab
  UI.dispatch
  assert(toggle.state.focused, 'tab focus')
end
check(results, 'UTF8 editing selection character limits readonly and numeric') do |canvas|
  input = UI::InputField.new(canvas)
  input.focus
  UI.text('你A🙂')
  assert(input.text == '你A🙂')
  UI.key(42) # backspace
  assert(input.text == '你A')
  UI.key(4, false, true) # ctrl+a
  UI.text('你好世界')
  assert(input.text == '你好世界')
  input.character_limit = 3
  UI.key(4, false, true)
  UI.text('🙂甲乙丙')
  assert(input.text == '🙂甲乙')
  input.read_only = true
  UI.key(42)
  UI.text('X')
  assert(input.text == '🙂甲乙')
  input.read_only = false
  input.text = ''
  input.character_limit = 0
  input.numeric = true
  UI.text('-12.5')
  UI.text('bad')
  assert(input.text == '-12.5')
end
check(results, 'scrollbar binding and pointer cancellation') do |canvas|
  panel = UI::ScrollPanel.new(canvas, size: [200, 100], content_size: Vector2.new(200, 500))
  bar = UI::ScrollBar.new(canvas, size: [20, 100])
  bar.position.x = 220
  bar.bind_scroll(panel)
  UI.update
  click(230, 88)
  assert(panel.scroll.y > 300, 'bar updates panel offset')
  UI.scroll(20, 20, 0, 1)
  UI.dispatch
  assert(near(bar.value, panel.scroll.y / 400.0), 'panel updates bar')
  button = UI::Button.new(panel)
  panel.scroll.y = 0
  canceled = 0
  button.on_pointer_cancel { canceled += 1 }
  UI.pointer('down', 20, 20, 7)
  UI.pointer('cancel', 20, 20, 7)
  UI.dispatch
  assert(canceled == 1 && !button.state.pressed)
  UI.pointer('down', 20, 20, 8)
  UI.pointer('down', 20, 20, 9)
  UI.pointer('up', 20, 20, 8)
  assert(button.state.pressed, 'second touch retains pressed state')
  UI.pointer('cancel', 20, 20, 9)
  assert(!button.state.pressed)
end
check(results, 'dropdown scrolling stops at visible final rows') do |canvas|
  dropdown = UI::Dropdown.new(canvas, items: Array.new(12) { |i| "item #{i}" })
  click(20, 20)
  UI.scroll(20, 90, 0, -100)
  assert(near(dropdown.scroll.y, 4 * dropdown.row_height), 'popup range uses eight visible rows')
  click(20, 40 + 7 * 32 + 16)
  assert(dropdown.selected_index == 11)
  dropdown.selected_index = -1
  dropdown.scroll.y = 0
  dropdown.expanded = true
  dropdown.focus
  8.times { UI.key(81) }
  assert(dropdown.selected_index == 7 && near(dropdown.scroll.y, 0), 'keyboard uses popup height rather than header height')
  UI.key(81)
  assert(dropdown.selected_index == 8 && near(dropdown.scroll.y, dropdown.row_height), 'keyboard scrolls only past last visible row')
end
check(results, 'list keypad navigation and submit preserve selection') do |canvas|
  list = UI::List.new(canvas, size: [200, 64], items: Array.new(20) { |i| "row #{i}" })
  list.focus
  submitted = 0
  list.on_submit { submitted += 1 }
  4.times { UI.key(90) } # keypad 2 navigates non-text controls with Num Lock on
  assert(list.selected_index == 3 && near(list.scroll.y, 64), 'keypad down reveals selected row')
  UI.key(40)
  UI.dispatch
  assert(list.selected_index == 3 && submitted == 1, 'enter submits current row without selecting first visible row')
  UI.key(96) # keypad 8
  assert(list.selected_index == 2, 'keypad up')
end
check(results, 'scrollbar thumb size grab offset resizing and direct scroll sync') do |canvas|
  panel = UI::ScrollPanel.new(canvas, size: [200, 100], content_size: Vector2.new(200, 500))
  bar = UI::ScrollBar.new(canvas, size: [20, 100])
  bar.position.x = 220
  bar.style.padding = 0
  bar.bind_scroll(panel)
  UI.update
  assert(near(bar.thumb_size, 0.2), 'visible fraction controls thumb length')
  panel.scroll.y = 200
  UI.update
  assert(near(bar.value, 0.5), 'direct panel scroll synchronizes bar')
  UI.pointer('down', 230, 42) # thumb spans 40..60, grab near its leading edge
  assert(near(panel.scroll.y, 200), 'grabbing thumb does not jump')
  UI.pointer('move', 230, 62)
  UI.pointer('up', 230, 62)
  assert(near(panel.scroll.y, 300), 'drag retains grab offset')
  bar.value = 1
  UI.update
  assert(near(panel.scroll.y, 400), 'programmatic bar value updates target')
  panel.height = 250
  UI.update
  assert(near(bar.thumb_size, 0.5) && near(bar.value, 1), 'resize recalculates visible fraction and clamps offset')
  panel.height = 500
  UI.update
  assert(near(bar.thumb_size, 1) && near(bar.value, 0), 'full content fills track')
  click(230, 80)
  assert(near(panel.scroll.y, 0), 'full thumb cannot scroll')
  bar.vertical = false
  bar.size = [100, 20]
  panel.content_size = Vector2.new(1000, 500)
  UI.update
  assert(near(bar.thumb_size, 0.2), 'horizontal binding uses width')
  click(310, 10)
  assert(panel.scroll.x > 600, 'horizontal thumb moves content')
  output = Bitmap.new(340, 520)
  canvas.render(output, Color.new(0, 0, 0))
  assert(output.get_pixel(225, 10).blue < 100, 'track before thumb has no progress fill')
  output.dispose
  panel.dispose
  UI.update
  assert(bar.scroll_target.nil?, 'disposed target releases binding')
end
check(results, 'input selection remains visible over a hovered skin and style edits refresh') do |canvas|
  input = UI::InputField.new(canvas, size: [200, 40], text: 'MMMMMMMM')
  input.style.font.outline = false
  input.style.font.shadow = false
  skin = Bitmap.new(16, 16)
  skin.fill_rect(0, 0, 16, 16, Color.new(30, 90, 130))
  input.style.skin = skin
  input.style.hover_skin = skin
  input.style.hover_color = Color.new(30, 90, 130)
  input.style.selection_color = Color.new(180, 40, 80)
  UI.pointer('move', 20, 20)
  input.focus
  UI.key(4, false, true)
  output = Bitmap.new(220, 80)
  canvas.render(output, Color.new(0, 0, 0))
  assert(output.get_pixel(10, 8).red > 150, 'selection differs from hover background')
  input.style.selection_color.green = 200
  canvas.render(output, Color.new(0, 0, 0))
  assert(output.get_pixel(10, 8).green > 180, 'selection color mutation invalidates cache')
  output.dispose
  skin.dispose
end
check(results, 'multiline caret home end and vertical movement') do |canvas|
  input = UI::InputField.new(canvas, multiline: true, text: "AB\nCD\nEF")
  input.focus
  input.cursor_position = 4
  input.selection_position = 4
  UI.key(74) # home
  assert(input.cursor_position == 3, "home cursor: #{input.cursor_position}, text #{input.text.inspect}")
  UI.key(77) # end
  assert(input.cursor_position == 5, "end cursor: #{input.cursor_position}")
  UI.key(82) # up
  assert(input.cursor_position == 2)
  UI.key(81, true) # shift+down
  assert(input.cursor_position == 5 && input.selection_position == 2)
  Graphics.update
end
check(results, 'mouse drag selects input text without scrolling its parent') do |canvas|
  panel = UI::ScrollPanel.new(canvas, content_size: Vector2.new(300, 500))
  input = UI::InputField.new(panel, text: 'ABCDEF')
  UI.pointer('down', 9, 20)
  UI.pointer('move', 160, 20)
  UI.pointer('up', 160, 20)
  assert(input.selection_position == 0 && input.cursor_position == 6)
  assert(near(panel.scroll.y, 0))
  UI.text('Z')
  assert(input.text == 'Z')
end
check(results, 'modal blocks background focus and interaction') do |canvas|
  input = UI::InputField.new(canvas)
  input.focus
  background_clicks = 0
  button = UI::Button.new(canvas)
  button.on_click { background_clicks += 1 }
  dialog = UI::Dialog.new(canvas, size: [200, 120])
  dialog.position = Vector3.new(300, 220, 0)
  UI.update
  assert(!input.state.focused, 'modal clears background focus')
  click(20, 20)
  assert(background_clicks == 0)
  confirm = UI::Button.new(dialog)
  UI.key(43)
  UI.dispatch
  assert(confirm.state.focused, 'tab contained in modal')
  dialog.visible = false
  click(20, 20)
  assert(background_clicks == 1)
end
check(results, 'callbacks can dispose and create nodes safely') do |canvas|
  button = UI::Button.new(canvas)
  replacement = nil
  button.on_click { button.dispose; replacement = UI::Text.new(canvas, text: 'Replaced') }
  click(20, 20)
  Graphics.update
  assert(button.disposed? && replacement.text == 'Replaced')
  assert(canvas.children.include?(replacement) && !canvas.children.include?(button))
end
check(results, 'parent cycles rejected and children survive Ruby GC') do |canvas|
  UI::Button.new(canvas, text: 'Retained')
  GC.start
  assert(canvas.children.first.text == 'Retained')
  button = canvas.children.first
  begin
    canvas.parent = button
    raise 'cycle accepted'
  rescue ArgumentError
  end
  assert(button.parent.equal?(canvas))
  canvas.dispose
  assert(button.disposed?)
end
check(results, 'nine-slice state sprites transparency and texture invalidation') do |canvas|
  assets = [[20, 40, 230], [230, 30, 40], [30, 220, 60], [230, 210, 30], [160, 40, 220]].map do |rgb|
    Bitmap.new(16, 16).tap { |bitmap| bitmap.fill_rect(0, 0, 16, 16, Color.new(*rgb)) }
  end
  normal, hover, pressed, disabled, focused = assets
  normal.fill_rect(0, 0, 16, 4, Color.new(230, 30, 40))
  normal.clear_rect(0, 0, 2, 2)
  button = UI::Button.new(canvas, size: [120, 50])
  style = UI::Style.new
  style.border_width = 0
  style.skin = normal
  style.hover_skin = hover
  style.pressed_skin = pressed
  style.disabled_skin = disabled
  style.focused_skin = focused
  style.slice = Vector4.new(4, 4, 4, 4)
  button.style = style
  output = Bitmap.new(200, 100)
  paint = -> { canvas.render(output, Color.new(0, 0, 0)); output.get_pixel(30, 20) }
  color = paint.call
  assert(color.blue > 200, 'normal sprite')
  assert(output.get_pixel(3, 2).red > 200, 'slice keeps source border width')
  corner = output.get_pixel(0, 0)
  assert(corner.red < 5 && corner.blue < 5, 'transparent corner stays transparent')
  UI.pointer('move', 20, 20)
  assert(paint.call.red > 200, 'hover sprite')
  UI.pointer('down', 20, 20)
  assert(paint.call.green > 200, 'pressed sprite')
  UI.pointer('cancel', 20, 20)
  UI.pointer('move', 180, 80)
  UI.focus(button)
  assert(paint.call.blue > 180 && paint.call.red > 140, 'focused sprite')
  button.interactable = false
  assert(paint.call.red > 200 && paint.call.green > 180, 'disabled takes priority')
  UI.focus(nil)
  button.interactable = true
  UI.pointer('move', 180, 80)
  normal.fill_rect(4, 4, 8, 8, Color.new(20, 210, 210))
  color = paint.call
  assert(color.green > 180 && color.blue > 180, 'source bitmap edit invalidates skin cache')
  style.hover_skin = nil
  UI.pointer('move', 20, 20)
  assert(paint.call.green > 180, 'missing state sprite falls back to normal')
  output.dispose
  assets.each(&:dispose)
end
File.write('regression.txt', results.join("\n") + "\n")
exit!
