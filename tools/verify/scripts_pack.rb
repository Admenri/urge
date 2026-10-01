# Pack and unpack an RGSS Scripts file the way cruby_main.cc reads it.
#
# The file is a Marshal dump of an array whose entries are
# [name, bookkeeping, zlib_compressed_source]. Only the name and the source are
# read by the engine, so replace keeps the middle element of an entry as it is.
#
# Usage:
#   ruby scripts_pack.rb list    <scripts file>
#   ruby scripts_pack.rb unpack  <scripts file> <output dir>
#   ruby scripts_pack.rb replace <scripts file> <index> <source .rb>
#
# Written against the ruby Marshal of the interpreter rather than a hand
# written one: the container is exactly what Marshal.dump produces, and a
# hand written reader or writer drifts from it in ways a round trip catches
# only by luck.

require "zlib"

def load_scripts(path)
  Marshal.load(File.binread(path))
end

def save_scripts(path, entries)
  # The bytes of the dump of a same value are stable across runs, so a file
  # written twice from the same input is byte identical and does not look
  # modified to a build system.
  File.binwrite(path, Marshal.dump(entries))
end

def entry_name(entry)
  entry[0].to_s
end

def cmd_list(path)
  load_scripts(path).each_with_index do |entry, index|
    source = entry[2]
    begin
      size = Zlib::Inflate.inflate(source).bytesize
    rescue Zlib::Error
      size = -1
    end
    printf("%3d  %-40s  stored=%6d  source=%7d\n",
           index, entry_name(entry), source.bytesize, size)
  end
end

def cmd_unpack(path, out_dir)
  require "fileutils"
  FileUtils.mkdir_p(out_dir)
  load_scripts(path).each_with_index do |entry, index|
    name = format("%03d_%s.rb", index, entry_name(entry))
    File.binwrite(File.join(out_dir, name), Zlib::Inflate.inflate(entry[2]))
  end
  puts "unpacked to #{out_dir}"
end

def cmd_replace(path, index, source_path)
  entries = load_scripts(path)
  index = Integer(index)
  source = File.binread(source_path)
  # Only the source of the entry changes. The name and the bookkeeping element
  # are left as the file had them, so the difference of the file is the one
  # intended and nothing else.
  entries[index][2] = Zlib::Deflate.deflate(source, Zlib::BEST_COMPRESSION)
  save_scripts(path, entries)
  puts "replaced entry #{index} (#{entry_name(entries[index])}) " \
       "with #{source.bytesize} bytes of source"
end

case ARGV[0]
when "list"    then cmd_list(ARGV[1])
when "unpack"  then cmd_unpack(ARGV[1], ARGV[2])
when "replace" then cmd_replace(ARGV[1], ARGV[2], ARGV[3])
else
  puts File.read(__FILE__).lines.grep(/^#/).map { |l| l.sub(/^# ?/, "") }.join
  exit 1
end
