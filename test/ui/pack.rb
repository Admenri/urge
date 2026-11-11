require 'zlib'
source = File.binread(ARGV.fetch(0))
dependencies = ARGV.drop(2).map { |path| File.binread(path) }
source = (dependencies + [source]).join("\n") unless dependencies.empty?
File.binwrite(ARGV.fetch(1), Marshal.dump([[1, 'Main', Zlib::Deflate.deflate(source)]]))
