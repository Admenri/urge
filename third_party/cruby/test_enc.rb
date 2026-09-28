# encoding: utf-8
puts "hello 中文 test"
File.open("enc_out.txt", "w:GBK") { |f| f.write("中文写入测试 OK\n") }
File.open("enc_bin.txt", "wb") { |f| f.write("binary OK\n") }
puts "file write done"
puts Fiber
