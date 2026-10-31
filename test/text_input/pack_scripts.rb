require 'zlib'

code = File.binread(ARGV[0])
scripts = [[1, 'Main', Zlib::Deflate.deflate(code)]]
File.binwrite(ARGV[1], Marshal.dump(scripts))
puts "wrote #{ARGV[1]} (#{code.bytesize} bytes)"
