# 静态 RGSS 编码模块

`RUBY_STATIC_RGSS_ENCODINGS` 默认开启（Windows、macOS、Linux、Android 和 WASM）。关闭后保留原来的核心编码构建。

补齐 `single_byte`、`utf_16_32`、`japanese_sjis` 及它们需要的 Onigmo 编码定义、别名和 JIS 属性表。支持 Ruby 原生 `String#encode/encode!`、字符长度/切片、编码有效性检查、正则、`Encoding::Converter` 分段转换和标准异常/替换选项。Shift_JIS 和 Windows-31J/CP932 分别保留，不将二者合并为一个系统代码页。

实现和映射使用当前内置 CRuby 4.0.0 的语义，不承诺复刻 Ruby 1.9.2 每个字符映射及别名细节。GBK/GB18030/Big5/EUC-JP 不在新增的三组转码模块中；Game.ini 的平台转码策略保持独立。

## 来源与构建

编码 C 文件、`iso_8859.h`、`shift_jis.h`、`utf_16_32.h`、`jis/props.h` 和三个生成的转码 C 文件直接取自 Ruby 4.0.0 官方发布包，revision 与仓库既有解释器一致：`553f1675f3a9cece340b90f374a4245dccac2272`。

- 发布包：https://cache.ruby-lang.org/pub/ruby/4.0/ruby-4.0.0.tar.xz
- SHA-256：`a72bacee9de07283ebc19baa4ac243b193129f21aa4e168c7186fb1fe7d07fe1`
- 字节保持上游原样；不新增运行时下载或构建时网络访问。

`RubyEncodings.cmake` 统一管理编译列表和初始化列表。编码对象以 `ONIG_ENC_REGISTER=rb_enc_register` 编译生成 `Init_*` 入口，转码对象以 `EXTSTATIC=1` 避免初始化符号冲突。`Init_enc` 先注册选定的数据库，再初始化实际编码和转码器，并提供对应的 `enc/*.so`、`enc/trans/*.so` feature 名称；无需运行时加载扩展。

`encdb_rgss.h` 来自同一版本的 `template/encdb.h.tmpl`，仅扫描仓库实际包含的 `enc/*.[ch]`。维护者新增编码时应重新生成数据库；跨平台构建直接使用已经生成的 C/头文件，不依赖宿主 Ruby 或 gperf。生成命令（发布包解压到 `$ruby_source`）：

```sh
ruby "$ruby_source/tool/generic_erb.rb" -o third_party/ruby/cmake/encdb_rgss.h \
  "$ruby_source/template/encdb.h.tmpl" third_party/ruby/enc
```

许可文本集中在 `ENCODINGS_LICENSES.txt`，包含 Ruby 的 COPYING/BSDL 和第三方 LEGAL 声明，随原生产物及 Android assets 一起分发。没有新增 iconv、ICU、JNI 或 Win32 转码依赖。
