# TextInput demo -- a small single-line text field driven by the platform IME.
#
# Run: copy Game.exe into this directory (or use run.bat), then start it.
# Type with the system input method; the preedit text is shown at the caret.
# Enter commits the line, Backspace/Delete edit it, Left/Right/Home/End move
# the caret, and Escape leaves the demo.
#
# The state lives in a class instance: the engine evaluates scripts with
# `self = nil`, so top-level instance variables are not writable.

$stdout.sync = true

WIDTH = Graphics.width
HEIGHT = Graphics.height
BOX_X = 40
BOX_Y = 96
BOX_W = WIDTH - 80
BOX_H = 52

SCAN_RETURN = 40
SCAN_ESCAPE = 41
SCAN_BACKSPACE = 42
SCAN_DELETE = 76
SCAN_LEFT = 80
SCAN_RIGHT = 79
SCAN_HOME = 74
SCAN_END = 77

class TextInputDemo
  def initialize
    @text = +''
    @cursor = 0
    @history = ''
    @status = 'Type with your input method. Enter commits the line; Esc quits.'

    @bitmap = Bitmap.new(WIDTH, HEIGHT)
    @bitmap.font.size = 24
    @bitmap.font.color = Color.new(236, 240, 248, 255)
    @sprite = Sprite.new
    @sprite.bitmap = @bitmap
    @sprite.z = 1000
  end

  def run
    TextInput.enable(Rect.new(BOX_X, BOX_Y, BOX_W, BOX_H))
    $stdout.puts 'TextInput demo ready (Esc quits)'
    $stdout.flush

    loop do
      Graphics.update
      Input.update
      pump_text_input
      handle_keys
      draw
    end
  ensure
    TextInput.disable rescue nil
  end

  private

  def draw
    @bitmap.clear
    @bitmap.fill_rect(0, 0, WIDTH, HEIGHT, Color.new(18, 22, 32, 255))
    @bitmap.fill_rect(BOX_X - 2, BOX_Y - 2, BOX_W + 4, BOX_H + 4,
                      Color.new(90, 140, 210, 255))
    @bitmap.fill_rect(BOX_X, BOX_Y, BOX_W, BOX_H, Color.new(8, 12, 20, 255))

    preedit = TextInput.composition
    before = @text[0...@cursor]
    after = @text[@cursor..] || ''
    field = "#{before}#{preedit}|#{after}"
    @bitmap.draw_text(BOX_X + 14, BOX_Y + 12, BOX_W - 28, 32, field, 0)

    @bitmap.font.size = 18
    @bitmap.font.color = Color.new(150, 168, 196, 255)
    @bitmap.draw_text(24, 20, WIDTH - 48, 28,
                      "TextInput demo  (active: #{TextInput.active?})", 0)
    @bitmap.draw_text(24, 50, WIDTH - 48, 28, @status, 0)
    @bitmap.draw_text(24, 166, WIDTH - 48, 26,
                      'Backspace/Delete edit, arrows move, Enter commits, Esc quits.',
                      0)
    @bitmap.draw_text(24, 196, WIDTH - 48, 26, "Last line: #{@history}", 0)
    @bitmap.font.size = 24
    @bitmap.font.color = Color.new(236, 240, 248, 255)
  end

  def handle_keys
    # Let the IME own editing keys while a preedit is active.
    return unless TextInput.composition.empty?

    if Input.key_trigger?(SCAN_ESCAPE)
      exit
    elsif Input.key_trigger?(SCAN_RETURN)
      @history = @text
      @status = @text.empty? ? 'Committed an empty line.' : "Committed: #{@text}"
      @text = +''
      @cursor = 0
      return
    end

    if Input.key_trigger?(SCAN_BACKSPACE) || Input.key_repeat?(SCAN_BACKSPACE)
      if @cursor > 0
        @text.slice!(@cursor - 1)
        @cursor -= 1
      end
    elsif Input.key_trigger?(SCAN_DELETE) || Input.key_repeat?(SCAN_DELETE)
      @text.slice!(@cursor) if @cursor < @text.length
    elsif Input.key_trigger?(SCAN_LEFT) || Input.key_repeat?(SCAN_LEFT)
      @cursor -= 1 if @cursor > 0
    elsif Input.key_trigger?(SCAN_RIGHT) || Input.key_repeat?(SCAN_RIGHT)
      @cursor += 1 if @cursor < @text.length
    elsif Input.key_trigger?(SCAN_HOME)
      @cursor = 0
    elsif Input.key_trigger?(SCAN_END)
      @cursor = @text.length
    end
  end

  def pump_text_input
    committed = TextInput.text
    return if committed.empty?

    @text.insert(@cursor, committed)
    @cursor += committed.length
    TextInput.clear
  end
end

TextInputDemo.new.run
