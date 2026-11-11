# Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
# Demo-only theme resources, similar to a Unity ScriptableObject theme.
class UIDemoTheme
  PALETTES = {
    blue: {name: '科技蓝', page: [16, 24, 36], panel: [25, 39, 57],
      button: [28, 68, 100], hover: [41, 103, 143], pressed: [17, 51, 79],
      disabled: [43, 52, 62], text: [223, 242, 255], accent: [67, 198, 242],
      border: [71, 134, 170], font: ['Microsoft YaHei UI', 'Microsoft YaHei']},
    paper: {name: '羊皮纸', page: [245, 237, 218], panel: [233, 217, 184],
      button: [213, 181, 128], hover: [236, 205, 152], pressed: [184, 144, 91],
      disabled: [205, 194, 168], text: [74, 49, 29], accent: [146, 83, 33],
      border: [143, 105, 59], font: ['SimSun']},
    pixel: {name: '像素绿', page: [15, 27, 19], panel: [24, 44, 29],
      button: [31, 67, 39], hover: [44, 96, 51], pressed: [21, 45, 27],
      disabled: [43, 54, 43], text: [209, 243, 176], accent: [123, 225, 108],
      border: [94, 157, 75], font: ['SimHei']},
    gold: {name: '独立金色', page: [28, 22, 12], panel: [54, 41, 18],
      button: [144, 97, 24], hover: [180, 129, 35], pressed: [106, 69, 16],
      disabled: [70, 61, 41], text: [255, 241, 196], accent: [255, 211, 91],
      border: [236, 186, 69], font: ['Microsoft YaHei UI', 'Microsoft YaHei']}
  }.freeze

  attr_reader :name, :key, :textures

  def initialize(key)
    @key = key
    @palette = PALETTES.fetch(key)
    @name = @palette[:name]
    @styles = {}
    @textures = {}
    Dir.mkdir('Skins') unless Dir.exist?('Skins')
    %i[panel button hover pressed disabled focused].each do |state|
      fill = state == :focused ? @palette[:button] : @palette[state]
      edge = state == :focused ? @palette[:accent] : @palette[:border]
      path = "Skins/#{key}_#{state}.png"
      # Existing files are artist overrides: replace a PNG to try your own art.
      @textures[state] = if File.file?(path)
        Bitmap.new(path)
      else
        make_texture(fill, edge).tap { |image| image.to_image.save_file(path) }
      end
    end
  end

  def color(name)
    Color.new(*@palette.fetch(name))
  end

  def style(role)
    @styles[role] ||= UI::Style.new.tap do |style|
      style.background = color(role == :page ? :page : :panel)
      style.foreground = color(:text)
      style.hover_color = color(:hover)
      style.pressed_color = color(:pressed)
      style.disabled_color = color(:disabled)
      style.accent = color(:accent)
      style.selection_color = color(:pressed)
      style.border_color = color(:border)
      style.font.name = @palette[:font]
      style.font.size = role == :title ? 28 : role == :caption ? 17 : 19
      style.font.outline = false
      style.font.shadow = false
      style.font.solid = @key == :pixel
      style.font.bold = @key == :pixel && role == :title
      style.padding = %i[text title caption page].include?(role) ? 0 : 6
      style.border_width = role == :input ? 1 : 0
      style.slice = Vector4.new(8, 8, 8, 8)
      case role
      when :button, :toggle, :input
        style.skin = @textures[:button]
        style.hover_skin = @textures[:hover]
        style.pressed_skin = @textures[:pressed]
        style.disabled_skin = @textures[:disabled]
        style.focused_skin = @textures[:focused]
      when :panel
        style.skin = @textures[:panel]
      end
    end
  end

  def dispose
    @textures.each_value { |image| image.dispose unless image.disposed? }
  end

  private

  def mix(a, b, fraction)
    Color.new(*a.zip(b).map { |x, y| (x + (y - x) * fraction).round })
  end

  def make_texture(fill, edge)
    Bitmap.new(48, 48).tap do |image|
      image.fill_rect(0, 0, 48, 48, Color.new(*edge))
      if @key == :pixel
        image.fill_rect(2, 2, 44, 44, Color.new(*fill))
      else
        image.gradient_fill_rect(2, 2, 44, 44, mix(fill, [255, 255, 255], 0.06),
          mix(fill, [0, 0, 0], 0.14), true)
      end
      image.fill_rect(2, 2, 44, 2, mix(edge, [255, 255, 255], 0.25))
      image.fill_rect(2, 2, 2, 44, mix(edge, [255, 255, 255], 0.15))
      image.fill_rect(2, 44, 44, 2, mix(edge, [0, 0, 0], 0.35))
      image.fill_rect(44, 2, 2, 44, mix(edge, [0, 0, 0], 0.25))
      # Keep the corners within the 8 px nine-slice borders.
      unless @key == :pixel
        [5, 3, 2, 1].each_with_index do |cut, y|
          image.clear_rect(0, y, cut, 1)
          image.clear_rect(48 - cut, y, cut, 1)
          image.clear_rect(0, 47 - y, cut, 1)
          image.clear_rect(48 - cut, 47 - y, cut, 1)
        end
      end
    end
  end
end
