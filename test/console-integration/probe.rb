# encoding: UTF-8
# Packed by run.py as the game's sole script. No gems or development bindings.
module ConsoleProbe
  @failures = []
  @apis = {}

  class << self
    def api(name, imports, result = 'I')
      @apis[[name, imports, result]] ||= Win32API.new('kernel32', name, imports, result)
    end

    def assert(value, message)
      raise message unless value
    end

    def check(name)
      yield
    rescue Exception => error
      # Never let a failed assertion reach the host's interactive error dialog.
      @failures << "#{name}: #{error.class}: #{error.message}".gsub(/[\r\n]/, ' ')
    end

    def enabled?
      @expected && @expected['enabled'] == '1'
    end

    def handle(index)
      # Native Win32API's 'P' result is a C string, not an opaque pointer. Windows
      # handles can be passed in their signed 32-bit representation on Win64;
      # 'P' imports then preserve the pointer-sized argument slots. The HWND is
      # used only for a nonzero test, never dereferenced or passed back to Windows.
      api('GetStdHandle', 'I', 'L').call(-10 - index)
    end

    def console_mode(handle)
      mode = "\0" * 4
      api('GetConsoleMode', 'PP').call(handle, mode) != 0
    end

    def standard_streams
      window = api('GetConsoleWindow', '', 'L').call
      assert((window != 0) == enabled?, "GetConsoleWindow=#{window}, expected console=#{enabled?}")
      [STDIN, STDOUT, STDERR].each_with_index do |stream, index|
        assert(stream.fileno == index, "standard fd #{index} became #{stream.fileno}")
        assert(!stream.closed?, "standard fd #{index} is closed")
        native = handle(index)
        assert(native != 0 && native != -1, "GetStdHandle(#{index}) is invalid")
        assert(api('GetFileType', 'P').call(native) == 2,
               "standard handle #{index} is not FILE_TYPE_CHAR (NUL/console)")
        assert(console_mode(native) == enabled?, "GetConsoleMode disagrees on fd #{index}")
        assert(stream.tty? == enabled?, "Ruby fd #{index} is not bound to the expected device")
      end
      if enabled?
        processes = "\0" * 64
        count = api('GetConsoleProcessList', 'PI').call(processes, 16)
        pid = api('GetCurrentProcessId', '', 'L').call
        assert(count == 1 && processes.unpack('L<*').first == pid,
               "console is shared instead of owned: count=#{count}")
      else
        # Never read enabled console input: that would require user interaction.
        assert($stdin.read(1).nil?, 'silent stdin did not return NUL EOF')
        fd0 = IO.for_fd(0, 'rb', :autoclose => false)
        assert(fd0.read(1).nil?, 'fd 0 did not return NUL EOF')
      end
    end

    def markers(stage)
      %w[RUBY_STDOUT RUBY_STDERR FD1 FD2 WINAPI_STDOUT WINAPI_STDERR].map do |name|
        "CONSOLE_#{name}_#{stage}"
      end
    end

    def write_output(stage)
      output = markers(stage)
      $stdout.sync = true
      $stderr.sync = true
      $stdout.puts(output[0])
      $stderr.puts(output[1])
      [1, 2].each_with_index do |fd, index|
        stream = IO.for_fd(fd, 'wb', :autoclose => false)
        text = output[2 + index] + "\n"
        assert(stream.syswrite(text) == text.bytesize, "fd #{fd} syswrite was short")
      end
      [1, 2].each_with_index do |fd, index|
        text = output[4 + index] + "\r\n"
        written = "\0" * 4
        ok = api('WriteFile', 'PPIPP').call(handle(fd), text, text.bytesize, written, 0)
        assert(ok != 0 && written.unpack('L<').first == text.bytesize,
               "WriteFile on fd #{fd} failed or was short")
      end
      $stdout.flush
      $stderr.flush
    end

    def snapshot(filename)
      info = "\0" * 22 # CONSOLE_SCREEN_BUFFER_INFO: COORDs, WORD, SMALL_RECT.
      output = handle(1)
      assert(api('GetConsoleScreenBufferInfo', 'PP').call(output, info) != 0,
             'GetConsoleScreenBufferInfo failed')
      width, height, _x, cursor_y = info.unpack('s<11')
      length = width * (cursor_y + 1)
      assert(width > 0 && height > 0 && length > 0 && length <= 131072,
             "console snapshot exceeds bound: #{width}x#{height}, cursor row #{cursor_y}")
      text = "\0" * (length * 2)
      read = "\0" * 4
      # COORD {0,0} is a by-value DWORD; strings are writable Win32API buffers.
      ok = api('ReadConsoleOutputCharacterW', 'PPIIP').call(output, text, length, 0, read)
      count = read.unpack('L<').first
      # Wide glyphs occupy multiple screen cells but are returned as characters.
      assert(ok != 0 && count > 0 && count <= length,
             "ReadConsoleOutputCharacterW failed: result=#{ok}, count=#{count}/#{length}")
      text = text.byteslice(0, count * 2).force_encoding('UTF-16LE').encode('UTF-8')
      File.write(filename, text)
      text
    end

    def once(text, marker)
      count = text.scan(Regexp.new(Regexp.escape(marker))).length
      assert(count == 1, "console must contain exactly one #{marker.inspect}, found #{count}")
    end

    def startup_messages(text)
      # These are emitted before the mounted Config activates an INI-only
      # console. CLI + INI + repeated CLI activation must not replay them twice.
      ['[Host] Game directory:', '[SDL] SDL revision:',
       '[Ruby] starting the interpreter'].each { |marker| once(text, marker) }
      if @expected['encoding'] != 'UTF-8'
        once(text, "[Config] Reading Game.ini as #{@expected['encoding']}")
      end
      if @expected['mounted'] == '1'
        once(text, '[Host] RGSS resource archive:')
      end
    end

    def run
      @expected = {}
      File.read('probe-expectations.txt').each_line do |line|
        key, value = line.strip.split('=', 2)
        @expected[key] = value
      end
      check('runtime switches') do
        {'debug' => !!$DEBUG, 'test' => !!$TEST, 'btest' => !!$BTEST}.each do |key, value|
          assert(value == (@expected.fetch(key) == '1'), "$#{key.upcase}=#{value}")
        end
      end
      check('CLI does not mutate the mapped INI boolean') do
        value = Config.get_value('Platform', 'WinConsole')
        assert(value == @expected.fetch('win_console'), "mapped WinConsole=#{value.inspect}")
      end
      check('main standard streams') { standard_streams }
      check('main stdout/stderr and fd writes') { write_output('MAIN') }
      check('live engine stderr logger') do
        # This documented rejected operation emits a warning but changes no INI
        # bytes. It covers the engine's CRT stderr path after Ruby initialization.
        Config.set_value('Game', 'IniEncoding', 'UTF-8')
      end
      if enabled?
        check('startup console replay') do
          text = snapshot('startup-screen.txt')
          startup_messages(text)
          markers('MAIN').each { |marker| once(text, marker) }
          once(text, '[Config] IniEncoding can only be changed before startup')
        end
      end
    end

    def finish
      # This runs inside ruby_cleanup, not before the script returns to the host.
      check('cleanup standard streams') { standard_streams }
      check('cleanup stdout/stderr and fd writes') { write_output('AT_EXIT') }
      if enabled?
        check('console remains writable through Ruby cleanup') do
          text = snapshot('cleanup-screen.txt')
          startup_messages(text)
          (markers('MAIN') + markers('AT_EXIT')).each { |marker| once(text, marker) }
          once(text, '[Config] IniEncoding can only be changed before startup')
        end
      end
      File.write('console-results.txt',
                 @failures.empty? ? "ALL_OK\n" : "FAIL\n#{@failures.join("\n")}\n")
    end
  end
end

# Register before assertions so their failures still exercise orderly teardown.
at_exit do
  ConsoleProbe.check('cleanup reporter') { ConsoleProbe.finish }
end
ConsoleProbe.check('probe setup') { ConsoleProbe.run }
