# Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
# Ruby convenience API. Native widgets remain direct subclasses of Node.
module UI
  Canvas = UICanvas
  Panel = UIPanel
  Image = UIImage
  Text = UIText
  Button = UIButton
  Toggle = UIToggle
  Slider = UISlider
  ProgressBar = UIProgressBar
  ScrollPanel = UIScrollPanel
  InputField = UIInputField
  LayoutGroup = UILayoutGroup
  Dropdown = UIDropdown
  List = UIList
  Dialog = UIDialog
  ScrollBar = UIScrollBar
  Separator = UISeparator
  RectTransform = UIRectTransform
  Style = UIStyle
  Raycaster = UIRaycaster

  HORIZONTAL = 1
  VERTICAL = 2
  GRID = 3
  STRETCH = 0
  CONTAIN = 1
  NINE_SLICE = 2
  CONSTANT_PIXEL_SIZE = 0
  FIT = 1
  SCALE_WITH_SCREEN_SIZE = 2

  @nodes = ObjectSpace::WeakMap.new
  class << self
    def register(node)
      @nodes[node] = node.state.id
    end

    def find_node(id)
      @nodes.each_pair { |node, key| return node if key == id && !node.disposed? }
      nil
    end

    # Called after the native frame has released its C++ render resources.
    def dispatch
      return if @dispatching
      @dispatching = true
      begin
        while (event = poll_event)
          node = find_node(event.target_id)
          next unless node
          event.instance_variable_set(:@target, node)
          while node && !node.disposed?
            event.instance_variable_set(:@current_target, node)
            node.__ui_emit(event) if node.respond_to?(:__ui_emit)
            break if event.stopped?
            node = node.parent
          end
        end
      ensure
        @dispatching = false
      end
    end
  end

  module WidgetAPI
    def initialize(parent = nil, **options)
      super(parent)
      @__ui_parent = parent
      @__ui_children = []
      @__ui_handlers = {}
      if parent
        children = parent.instance_variable_get(:@__ui_children)
        parent.instance_variable_set(:@__ui_children, children = []) unless children
        children << self
      end
      UI.register(self)
      options.each { |name, value| public_send("#{name}=", value) }
      yield self if block_given?
    end

    def state
      @__ui_state ||= super
    end

    def rect_transform
      @__ui_rect_transform ||= state.rect_transform
    end

    def parent
      @__ui_parent || super
    end

    def parent=(value)
      ancestor = value
      while ancestor
        raise ArgumentError, 'Node parent cycle' if ancestor.equal?(self)
        ancestor = ancestor.parent
      end
      raise ArgumentError, 'parent is disposed' if value && value.disposed?
      super(value)
      @__ui_parent&.instance_variable_get(:@__ui_children)&.delete(self)
      @__ui_parent = value
      if value
        children = value.instance_variable_get(:@__ui_children)
        value.instance_variable_set(:@__ui_children, children = []) unless children
        children << self unless children.include?(self)
      end
      value
    end

    def children
      (@__ui_children || []).reject(&:disposed?).dup
    end

    def dispose
      return if disposed?
      (@__ui_children || []).dup.each { |child| child.dispose unless child.disposed? }
      @__ui_children&.clear
      @__ui_parent&.instance_variable_get(:@__ui_children)&.delete(self)
      @__ui_parent = nil
      @__ui_handlers&.clear
      super
    end

    def on(type, callable = nil, &block)
      handler = callable || block
      raise ArgumentError, 'callback required' unless handler.respond_to?(:call)
      (@__ui_handlers[type.to_s] ||= []) << handler
      handler
    end

    def off(type, handler = nil)
      handler ? @__ui_handlers[type.to_s]&.delete(handler) : @__ui_handlers.delete(type.to_s)
      self
    end

    def __ui_emit(event)
      (@__ui_handlers[event.type] || []).dup.each do |handler|
        handler.call(event)
        break if event.stopped? || disposed?
      end
    end

    def focus
      UI.focus(self)
      self
    end

    def style
      state.style
    end

    def style=(value)
      state.style = value
    end

    def width
      rect_transform.size.x
    end

    def width=(value)
      rect_transform.size.x = value
    end

    def height
      rect_transform.size.y
    end

    def height=(value)
      rect_transform.size.y = value
    end

    def size=(value)
      rect_transform.size = value.is_a?(Array) ? Vector2.new(*value) : value
    end

    def anchor=(value)
      rect_transform.anchor = value
    end

    def text=(value)
      state.text = value
      if is_a?(UIInputField)
        state.cursor_position = value.bytesize
        state.selection_position = value.bytesize
      end
    end

    # Bind a scrollbar to a scroll panel. Both still use the same Node tree.
    def bind_scroll(panel, axis = :vertical)
      raise ArgumentError, 'expected UIScrollBar' unless is_a?(UIScrollBar)
      raise ArgumentError, 'expected ScrollPanel or List' unless panel.is_a?(UIScrollPanel) || panel.is_a?(UIList)
      self.vertical = axis == :vertical
      self.scroll_target = panel
      self
    end
  end

  properties = %i[interactable raycast_target blocks_raycasts focusable modal clip_children
    double_sided opacity text placeholder bitmap image_mode alignment vertical_alignment
    word_wrap checked value min_value max_value step vertical scroll content_size scroll_speed
    multiline password read_only numeric character_limit layout_mode spacing padding cell_size
    columns items selected_index row_height expanded reference_size scale_mode match_width_or_height
    cursor_position selection_position thumb_size scroll_target]
  properties.each do |name|
    WidgetAPI.define_method(name) { state.public_send(name) }
    next if name == :text
    WidgetAPI.define_method("#{name}=") { |value| state.public_send("#{name}=", value) }
  end
  %i[click value_changed selection_changed submit cancel focus blur pointer_enter pointer_exit
     pointer_down pointer_up pointer_move pointer_cancel drag scroll].each do |name|
    WidgetAPI.define_method("on_#{name}") { |callable = nil, &block| on(name, callable, &block) }
  end
  [Canvas, Panel, Image, Text, Button, Toggle, Slider, ProgressBar, ScrollPanel,
   InputField, LayoutGroup, Dropdown, List, Dialog, ScrollBar, Separator].each do |type|
    type.prepend(WidgetAPI)
  end

  module FrameAPI
    def update(*args)
      result = super
      UI.dispatch
      result
    end
  end
  singleton_class.prepend(FrameAPI)
  Graphics.singleton_class.prepend(FrameAPI)
end

class UIRectTransform
  ANCHORS = {top_left: [0, 0], top: [0.5, 0], top_right: [1, 0],
    left: [0, 0.5], center: [0.5, 0.5], right: [1, 0.5],
    bottom_left: [0, 1], bottom: [0.5, 1], bottom_right: [1, 1]}.freeze

  def anchor=(preset)
    if preset == :stretch
      self.anchor_min = Vector2.new(0, 0)
      self.anchor_max = Vector2.new(1, 1)
      self.pivot = Vector2.new(0, 0)
      self.size = Vector2.new(0, 0)
    else
      point = ANCHORS.fetch(preset)
      self.anchor_min = Vector2.new(*point)
      self.anchor_max = Vector2.new(*point)
      self.pivot = Vector2.new(*point)
    end
  end
end

class UIEvent
  attr_reader :target, :current_target
  alias stopped? stopped
end

class UIRaycastHit
  alias __native_target target
  def target
    UI.find_node(target_id) || __native_target
  end
end
