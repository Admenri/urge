# RGSS1/2/3 Ruby 兼容层

本目录集中保存旧 Ruby API 补齐、行为恢复和相关文档。初始化入口为 `InitLegacyRubyCompat(rgss_version)`，脚本求值入口为 C++ 的 `EvalRubyScript`；宿主只负责调用和捕获异常。没有全局 Ruby 求值辅助函数，也不在游戏目录装载额外补丁文件。测试在仓库外进行，本目录只保留运行时代码。

本兼容层的目标仅为：使用 CRuby 4.0 运行 RGSS1/2/3 的旧脚本。不要求旧 RGSS 运行现代 Ruby 脚本。positional Hash / keyword 参数适配不在范围内。API 补齐和本文明确列出的固定值兼容在初始化时安装；类声明行为恢复仍遵循“先按 CRuby 4.0 执行，发生明确错误后再尝试有限 fallback”的约定。

- `legacy_ruby_compat.cc/.h`：版本开关、缺失 API 清单和求值分派。
- `legacy_classes.cc/.h`：配置类继承冲突及整数别名声明的异常后恢复；Ruby 实现也嵌在此文件内。
- [CLASS_FALLBACK.md](CLASS_FALLBACK.md)：已有类恢复的触发条件和限制。

## 版本基线

| 游戏运行时 | 对应产品 | 原始 Ruby 基线 | 与本项目 CRuby 4.0 的关系 |
| --- | --- | --- | --- |
| RGSS1 | RPG Maker XP | Ruby 1.8.1 | 启用本目录的 API 补齐和有限行为恢复 |
| RGSS2 | RPG Maker VX | Ruby 1.8.1 | Ruby 语言层与 RGSS1 采用相同基线；不应误认为是 Ruby 1.9 |
| RGSS3 | RPG Maker VX Ace | Ruby 1.9.2 | 补齐 1.9.2 中已有、后来移除的 API；保持现代行为，不启用 1.8 类恢复 |

XP 的版本有 [Matz 当时的记录](https://matz.rubyist.net/0717.html)；VX 试用版的[同期实测记录](https://ktakaki.hatenablog.com/entry/20071206/p2)也明确指出 1.8.1。RGSS3 的 1.9.2 基线见 [VX Ace 官方产品页](https://www.rpgmakerweb.com/products/rpg-maker-vx-ace)。RGSS 是经过厂商修改的嵌入运行时：这些版本号不能证明每个 DLL 都与某个 MRI 标签逐字节等价。以下 API 语义分别对照 Ruby 官方的 [`v1_8_1` 源码](https://github.com/ruby/ruby/tree/v1_8_1)和 [`v1_9_2_0` 源码](https://github.com/ruby/ruby/tree/v1_9_2_0)，结合厂商参考手册和本仓库所带的 CRuby 4.0.0 实际验证；不以 1.8.7 文档代替 1.8.1。

RGSS1 与 RGSS2 对比 CRuby 4.0 的主要 **Ruby 语言差异是共用的**。两者的引擎接口差别应另行处理，例如 XP 的 `$DEBUG` 与 VX 的 `$TEST`、默认帧率、图块系统等；它们不是 Ruby 版本升级造成的差异。[VX 厂商手册的版本差异说明](https://www.rpg-maker.fr/dl/monos/aide/vx/source/rgss/rgss.html)列出了这些引擎变化。

## 兼容原则

1. **API 缺失**：只补有明确旧版语义、可以独立恢复的名字。已有 public、protected、private 方法，以及常量和 autoload 声明都优先保留。
2. **行为差异**：默认保持 CRuby 4.0。只有确实发生对应异常、能识别具体旧模式并安全恢复时才 fallback；不符合条件或恢复后仍失败，正常上报错误。
3. **没有异常的语义变化**：不能靠异常 fallback 自动发现；记录差异，不擅自选择旧行为。若以后引入按游戏或调用点显式启用的兼容模式，必须单独评审。
4. 不通用 `rescue StandardError` 后重跑整个脚本、方法或游戏循环。错误之前可能已经修改全局状态、写文件或执行别名定义，重跑会重复副作用。
5. 兼容规则、版本条件、恢复日志均留在本目录；不向各个绑定、游戏数据类或厂商 Ruby 源码散布补丁。继续报告真实 `RUBY_VERSION`。

## 本轮已补齐的 API

按原始 Ruby 基线选择 API，不把 RGSS3 的兼容层整体关闭。方法和常量只在名称缺失时安装；`$SAFE` 采用下述固定值兼容策略。错误参数继续由原生方法检查，不吞异常；旧版的弃用警告不复刻。

| 旧 API | RGSS1/2（1.8.1） | RGSS3（1.9.2） | 实现语义与关键边界 |
| --- | --- | --- | --- |
| `File.exists?` | 补齐 | 补齐 | 为 `File.exist?` 安装别名，保持路径转换、文件/目录检查及原生异常 |
| `FileTest.exists?` | 补齐 | 补齐 | 为 `exist?` 安装别名，同时恢复模块函数和 mixin 的 private 方法 |
| `Dir.exists?` | 不添加 | 补齐 | 为 `Dir.exist?` 安装别名；检查目录，普通文件返回 false |
| `Hash#index` | 补齐 | 补齐 | 为 `key` 安装别名，按值查键；多个相等值沿用现代 Hash 顺序 |
| `ENV.index` | 补齐 | 补齐 | 为 `ENV.key` 安装别名，按环境变量值查名称；不改变环境变量 |
| `TRUE`、`FALSE`、`NIL` | 补齐 | 补齐 | 恢复为 `true`、`false`、`nil`；不覆盖常量，不触发已有 autoload |
| `Fixnum`、`Bignum` | 补齐 | 补齐 | 两个常量均别名到 `Integer`，支持旧类名引用、类型检查和类扩展；不覆盖已有常量或 autoload |
| `Object#taint`、`Object#untaint`、`Object#tainted?` | 补齐 | 补齐 | 前两者为空操作，返回接收者；查询固定为 false，不恢复污点机制 |
| `Object#trust`、`Object#untrust`、`Object#untrusted?` | 不添加 | 补齐 | 前两者为空操作，返回接收者；查询固定为 false，不恢复信任机制 |
| `$SAFE` | 固定值兼容 | 固定值兼容 | 读取始终为整数 0，写入忽略，不启用安全级别限制 |
| `Object#type`、`Object#id` | 补齐 | 不添加 | 在 Kernel 中为 `class`、`object_id` 安装别名；不伪造旧 VM 地址布局 |
| `Array#nitems` | 补齐 | 不添加 | 统计非 nil 元素，false 计数；不调用覆写的 `each`，不执行 block |
| `Array#indices`、`Array#indexes` | 补齐 | 不添加 | 每个参数分别进行原生 `Array#[]` 读取；Range 保留为一个嵌套切片 |
| `Hash#indices`、`Hash#indexes` | 补齐 | 不添加 | 为 `values_at` 安装别名，保持缺失键的默认值/default proc；Range 是普通键 |

旧版依据：Ruby 1.8.1 的 [file.c](https://github.com/ruby/ruby/blob/v1_8_1/file.c)、[object.c](https://github.com/ruby/ruby/blob/v1_8_1/object.c)、[array.c](https://github.com/ruby/ruby/blob/v1_8_1/array.c)、[hash.c](https://github.com/ruby/ruby/blob/v1_8_1/hash.c)，以及 Ruby 1.9.2 的 [file.c](https://github.com/ruby/ruby/blob/v1_9_2_0/file.c)、[dir.c](https://github.com/ruby/ruby/blob/v1_9_2_0/dir.c)、[hash.c](https://github.com/ruby/ruby/blob/v1_9_2_0/hash.c)、[object.c](https://github.com/ruby/ruby/blob/v1_9_2_0/object.c)。1.9.2 已不注册表内仅适用于 1.8 的名字，不向 RGSS3 顺带补入它们。`TRUE`、`FALSE`、`NIL` 在 Ruby 2.4 被弃用，在 [Ruby 3.0](https://docs.ruby-lang.org/en/3.0/NEWS_md.html) 正式移除；小写的 `true`、`false`、`nil` 不受影响。`File.exists?`、`Dir.exists?` 则在 [Ruby 3.2](https://docs.ruby-lang.org/en/4.0/NEWS/NEWS-3_2_0_md.html) 移除。

```ruby
[:a, :b, :c].indices(0..1, -1)   # => [[:a, :b], :c]
[:a, :b, :c].values_at(0..1, -1) # => [:a, :b, :c]，保持现代语义
```

`File.exist?` 本身仍存在，缺的是带 s 的旧拼写；`Array#collect`/`map` 本身也仍存在，不能当成缺失 API 重定义。`Dir.exists?` 不在 MRI 1.8.1 的 Dir API 中，但存在于 1.9.2，因此只为 RGSS3 添加。

## taint / trust 与 `$SAFE`

RGSS1/2/3 均补齐 `Object#taint`、`untaint`、`tainted?`；RGSS3 额外补齐其 Ruby 1.9.2 基线中的 `trust`、`untrust`、`untrusted?`。已有方法（包括 protected/private）优先保留。所有新增方法均接受零个参数：状态修改方法直接返回接收者，包括冻结对象和立即值；查询固定返回 `false`，不设置对象标志，也不传播污点或信任状态。链式调用如 `text.untaint.strip` 可继续执行；错误参数数量仍按 Ruby 规则报错。

`$SAFE` 在有效 RGSS 模式初始化时注册为虚拟全局变量，读取始终为整数 `0`。任何赋值均接受并忽略，不转换传入对象、不触发旧版安全级别检查，也不保存线程或全局状态。例如 `$SAFE = 4` 之后再次读取仍为 `0`；赋值表达式本身按 Ruby 语法返回右值 `4`。在新线程、Fiber 和 VM 重置后读取也为 `0`。此处不使用只读变量，以免旧脚本正常赋值时报错。

这是明确的无机制兼容，仅用于旧脚本调用，不提供沙箱，也不还原依赖 `$SAFE`/taint 的旧 VM 安全行为。空操作语义可参照 [Ruby 2.7 的 object.c](https://github.com/ruby/ruby/blob/v2_7_0/object.c) 与 [Ruby 官方移除讨论 #16131](https://bugs.ruby-lang.org/issues/16131)；`$SAFE` 固定为 0 并忽略写入是 URGE 的兼容策略，不等同于后来 Ruby 中可任意赋值的普通全局变量。

## 运行时常量与宿主初始化

`ARGV`、`STDIN`、`STDOUT`、`STDERR` 属于运行时初始化和宿主 I/O 语义的核查范围。它们不是被 Ruby 2.x/3.x 删除的旧名称，不能像 `File.exists?` 一样直接归入“缺失 API”。标准名称为 `STDOUT`，不添加 `STDOU` 等拼写别名。

| 对象 | 当前 URGE 嵌入运行时 | 兼容处理与边界 |
| --- | --- | --- |
| `ARGV` | `ruby_init()` 创建 Array；当前没有调用 `ruby_set_argv`，实际游戏启动时为 `[]`，即使宿主收到了参数 | 保留原数组。引擎的 `Config::SetCommandLine` 单独保存启动参数；是否向脚本转交参数以及怎样过滤引擎开关，属于另行定义的宿主契约，不在本轮自动改变 |
| `$*`、`ARGF.argv` | 与 `ARGV` 引用同一数组 | 不另造数组来补名；自动把启动开关塞进 ARGV 还可能使 ARGF/gets 将其作为文件名处理 |
| `STDIN`、`STDOUT`、`STDERR` | CRuby 的 I/O 初始化创建原始标准流对象 | 保留真实对象。常量存在不保证 GUI、移动端或 Web 宿主有可交互的控制台或可用的同类文件描述符；不能用无效占位流伪装支持 |
| `$stdin`、`$stdout`、`$stderr` | 初始化时指向对应标准流，随后可以重定向 | 它们表示当前流。重新赋值 `$stdout` 不会让 `STDOUT` 自动跟随；不把常量重新绑定到当前全局变量 |

当前 CRuby 的依据见仓库内 `third_party/ruby/ruby.c` 的 `ruby_prog_init`/`ruby_set_argv`，以及 `third_party/ruby/io.c` 的 I/O 初始化；全局变量与标准流的关系见 [Ruby 运行时文档](https://docs.ruby-lang.org/en/4.0/language/globals_md.html)。MRI 1.8.1 的 [io.c](https://github.com/ruby/ruby/blob/v1_8_1/io.c) 和 MRI 1.9.2 的 [io.c](https://github.com/ruby/ruby/blob/v1_9_2_0/io.c) 也注册原始标准流常量。

仓库外已检查兼容层初始化前后这些常量的对象身份、ARGV 别名和 stdout 重定向，并在 macOS 实际启动 RGSS1/2/3 模式，确认常量均存在、ARGV 为空。上述结果证明的是当前 URGE 的宿主行为；厂商各版 RGSS DLL 的暴露情况及控制台行为仍需原版运行时探针验证，不能只凭 MRI 的版本号推断。当前不新增这几个常量的 shim，也不改变启动参数或标准流策略。

## 行为差异清单与处理方案

此表对 **RGSS1 → CRuby 4.0** 和 **RGSS2 → CRuby 4.0** 分别成立；两者都基于 1.8.1。它是与游戏脚本相关的重点清单，不声称覆盖二十余年 Ruby 的全部变更。下表明确区分已有实现和方案/限制；未标为已实现的行为差异不安装全局补丁。

| 差异 | Ruby 1.8.1 / RGSS1、RGSS2 基线 | CRuby 4.0 | 本轮状态 / 后续方案 |
| --- | --- | --- | --- |
| 同名类重新指定不同父类 | MRI 1.8.1 的 `NODE_CLASS` 分支可重新绑定类常量 | `TypeError: superclass mismatch` | **已有有限 fallback**：只恢复满足条件的配置类，保留配置常量，从失败声明继续；不仿真任意活动类替换 |
| 无 block 的 `Array#collect/map`、`Enumerable#collect/map` | 返回数组 | 返回 Enumerator | 保持现代返回值；具体兼容方案见下一节 |
| 无 block 的 `each`、`select` 等 | 非空集合通常在第一次 yield 时抛 `LocalJumpError`；空集合可能直接返回 | 很多方法返回 Enumerator | 不把所有 Enumerator 统一转成数组，也不凭“迭代器”一词概括旧版语义 |
| 带 block 的 `Hash#select` | 返回 `[key, value]` 对组成的 Array | 返回 Hash | 保留现代行为；即使完全不报错也可能算出不同结果，不能自动判断意图 |
| `String#[](整数)`、`?a` | 字节整数，例如 97 | 单字符 String，例如 `"a"` | 保持现代行为；已确认的字节算法应在具体调用点使用 `getbyte` 等，不全局替换字符串索引 |
| `IO#getc`（包括 `STDIN.getc`） | 返回字节整数或 nil | 返回单字符 String 或 nil；`getbyte` 用于字节读取 | 保留现代行为；这是方法返回值差异，不能通过补标准流常量解决；RGSS3 的 1.9.2 已采用字符语义 |
| String 长度、切片、正则和编码 | 字节串模型；RGSS 文本使用 UTF-8，`$KCODE` 参与旧正则规则 | 编码感知的字符操作，有编码兼容性异常 | 不全局强制二进制或恢复 `$KCODE` 为有效开关；具体报错须区分文本与二进制数据后处理 |
| String 的 Enumerable 接口 | String 混入 Enumerable，`each` 按行迭代 | 不再混入 Enumerable，`each` 缺失 | **候选，未实现**；需一起核对按行/无 block/返回值语义，不能简单给 String 全局 include Enumerable |
| 反射结果 | `methods`、`instance_methods`、`constants` 等以字符串命名 | 通常返回 Symbol | 保留现代结果；在已确认的旧消费者处转换，不全局改反射 API |
| block 形参作用域 | 可覆盖同名外部局部变量 | block 形参有独立作用域 | 大多不报错，无法通过异常恢复；需要显式脚本级迁移方案 |
| Hash 遍历顺序 | 不承诺现代插入顺序 | 保持插入顺序 | 不复刻旧散列表布局；不得承诺重复值查键的旧遍历结果 |
| `Fixnum` / `Bignum` | 两个可见整数类，原版 RGSS 为 32 位宿主 | 统一为 Integer，旧常量在所带 4.0 中缺失 | **已实现别名**：两者均指向 Integer，按兼容策略合并旧类名；`class Fixnum/Bignum < Integer` 在对应异常后按无显式父类的重新打开处理，不复刻原版整数分类 |
| Proc/block 参数展开、隐式捕获 block | 部分规则与现代不同 | 形参数量/展开更严格；隐式 `Proc.new` 捕获不可依赖 | 保持现代行为；有明确旧调用点、原始参数和可重复调用保证时，才评估局部适配 |
| 老式 `retry` 等语法 | 1.8 支持某些迭代上下文 | 现代编译可抛 SyntaxError | **候选，未实现**；只能在编译失败后按明确语法分析，不用正则盲改字符串或整份源码 |
| `$SAFE`、taint / trust | 依赖旧 VM 的安全级别、污点和信任机制 | 机制已移除 | **已提供无机制兼容**：taint / trust 修改返回接收者、查询为 false；`$SAFE` 恒为 0，写入忽略，不恢复旧安全限制 |
| 线程模型、原生扩展 ABI | 依赖旧 VM 的线程实现和 C ABI | 机制已变化；原生扩展 ABI 不兼容 | 保留现代线程模型，不提供旧 Ruby C ABI；需要各自独立设计 |

上表旧语义可定位到官方 1.8.1 的 [eval.c](https://github.com/ruby/ruby/blob/v1_8_1/eval.c)（类声明、block、反射）、[enum.c](https://github.com/ruby/ruby/blob/v1_8_1/enum.c)、[array.c](https://github.com/ruby/ruby/blob/v1_8_1/array.c)、[hash.c](https://github.com/ruby/ruby/blob/v1_8_1/hash.c)、[string.c](https://github.com/ruby/ruby/blob/v1_8_1/string.c)、[io.c](https://github.com/ruby/ruby/blob/v1_8_1/io.c) 和 [parse.y](https://github.com/ruby/ruby/blob/v1_8_1/parse.y)。RGSS 文本默认 UTF-8 的厂商说明见 [XP 手册](https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/rgss.html#character_code)及 [VX 手册](https://www.rpg-maker.fr/dl/monos/aide/vx/source/rgss/rgss.html#character_code)。现代对应语义以仓库内 `third_party/ruby/` 为准，另见 [Ruby 4.0 Array 文档](https://docs.ruby-lang.org/en/4.0/Array.html)、[Ruby 2.4 整数类合并说明](https://www.ruby-lang.org/en/news/2016/12/25/ruby-2-4-0-released/)和 [Ruby 3.0 参数变化说明](https://www.ruby-lang.org/en/news/2020/12/25/ruby-3-0-0-released/)。

## RGSS3 → CRuby 4.0

RGSS3 的 Ruby 1.9.2 已采用 Enumerator、字符型字符串索引、Hash 型 `Hash#select` 结果和独立 block 形参作用域；上述 1.8 → 1.9 行为差异不应再次为它启用。后来的 Ruby 2.x/3.x 仍有 API 删除与行为调整，因此不能把“无需 1.8 行为恢复”等同于“无需 API 兼容”。

RGSS3 安装上表中标为“补齐”的 API；求值先执行原始源码，仅在匹配的异常后启用整数别名类声明恢复，不启用 RGSS1/2 的配置类恢复。`Fixnum` 和 `Bignum` 在此模式下也都别名到 Integer，允许在此边界内恢复旧式的 `< Integer` 声明。`$SAFE` 与 taint / trust 采用上文的无机制兼容。Proc/block 调用规则仍按上表中的边界评估，不做通用异常重试；positional Hash / keyword 参数不做适配。

## String / Symbol 转换

显式的 `String#to_sym/intern` 与 `Symbol#to_s/id2name` 在原始 Ruby 基线和所带 CRuby 4.0 中都存在，普通名称的双向转换无需补 API。须区分下列变化：

| 项目 | 已核实的差异 | 当前处理 |
| --- | --- | --- |
| 常规显式转换 | `"foo".to_sym == :foo`、`:foo.to_s == "foo"` 在旧版和当前版本均成立 | 直接使用现有方法，不重定义 |
| 空串、NUL 的符号化 | MRI 1.8.1 的 `to_sym/intern` 拒绝空串及含 NUL 的字符串；1.9.2 和当前版本允许 | 当前更宽的输入范围不妨碍旧脚本成功执行，不复刻旧版本的拒绝行为 |
| Symbol 转 String 后修改 | 旧版返回可修改字符串；所带 CRuby 4.0 的 `to_s/id2name` 返回 chilled 字符串，仍可修改，开启弃用警告时可能提示 | 没有发生修改失败，无需补丁；不把其他版本中返回 frozen 字符串的变更套到本项目 |
| 编码 | 1.8.1 是字节串模型；1.9 引入编码语义，当前 `to_sym` 对无效编码仍可能报错 | 遇到真实旧脚本失败后结合来源编码评估，不通用强制二进制或吞掉 EncodingError |
| 隐式字符串转换及比较 | `to_s` 不等于 `to_str`；Symbol 并不因此自动成为 String，`:foo == "foo"` 不成立 | 不给 Symbol 补 `to_str`，不修改 String/Symbol 比较、拼接或 Hash 键等价规则 |
| 反射 API 的结果 | 1.8.1 的 `methods/instance_methods/constants` 等返回字符串名字；1.9.2 和当前版本返回 Symbol 名字 | 这是返回值行为变化，不是互转方法缺失；旧消费者处显式转换或有依据的局部恢复须单独评估 |
| Symbol 的生命周期 | Ruby 2.2 开始回收符合条件的动态 Symbol | 保留当前 GC；仍被旧脚本正常引用的 Symbol 不需要通过常驻表保活 |

依据：MRI 1.8.1 的 [string.c](https://github.com/ruby/ruby/blob/v1_8_1/string.c) 和 [object.c](https://github.com/ruby/ruby/blob/v1_8_1/object.c)、MRI 1.9.2 的 [string.c](https://github.com/ruby/ruby/blob/v1_9_2_0/string.c)、仓库内 `third_party/ruby/string.c` 的 `rb_sym_to_s` 和 `third_party/ruby/symbol.c` 的 `rb_str_intern`。另见 [Symbol#to_s 的变更讨论](https://bugs.ruby-lang.org/issues/20350)及 [Ruby 2.2 Symbol GC 说明](https://www.ruby-lang.org/en/news/2014/12/25/ruby-2-2-0-released/)。以上以所带 CRuby 4.0 的源码和实测结果为准，不照搬后续版本文档。

本轮仅补充差异清单；未添加 String/Symbol 互转补丁。

## Enumerator：保留默认行为，不能统一转 Array

问题不只影响 RGSS1：RGSS2 的 Ruby 基线同样是 1.8.1。准确的例子是：

| 表达式 | Ruby 1.8.1 | 本项目默认 CRuby 4.0 |
| --- | --- | --- |
| `[1, 2].collect` / `.map` | 新 Array `[1, 2]` | Enumerator |
| `(1..2).collect` | Array `[1, 2]` | Enumerator |
| `[1, 2].collect { \|x\| x * 2 }` | Array `[2, 4]` | Array `[2, 4]` |
| `[1, 2].each` | 首次 yield 时抛 LocalJumpError | Enumerator |
| `[].each` | 返回原 Array | Enumerator |
| `{a: 1}.select { true }` | Array `[[:a, 1]]` | Hash `{a: 1}` |

当前没有覆盖 `collect/map/each`，也没有修改 Enumerator 的类型判断、`to_ary` 或 `method_missing`。后续方案以旧脚本实际失败的恢复能力为依据；默认执行 CRuby 4.0 语义来自异常后 fallback 的约定。

“先运行现代行为、报错才回退”有两条必须承认的边界：

- `result.is_a?(Array)` 返回 false、Hash/Array 的 `size` 恰好相同等情况没有错误，无法自动发现旧意图。
- 到宿主捕获到未处理异常时，Ruby 已经展开调用栈。不能假装可以在任意方法内部从原指令继续执行；重跑整段会重复副作用。

后续若实现自动恢复，应先收集真实失败样例，再做 **生产方法 + 消费调用点的白名单**：确认 Enumerator 来自原生 `Array#map/collect` 或明确有限的数据源；只适配已识别的旧消费者，不靠异常消息猜测；消费者失败后仍有接收者和参数可用，并且能只重试这一项操作时才转换。禁止重复枚举带副作用的数据源，也禁止对任意 Enumerator 调用 `to_a`，因为它可能无限或过大。

需要保持“游戏自己的 rescue 优先”时，调用点内部自动拦截也不能直接使用：必须另外评估异常如何到达宿主，以及是否存在安全续执行边界。无法证明安全时继续报告原错误。现有类恢复能处理的只是受约束的顶层声明，不能据此推广成任意运行时异常的恢复器。

若某个游戏需要连无异常的返回类型差异也恢复，唯一明确的方案是另行设计可选择、按游戏或调用点启用的旧语义模式，或者明确迁移相关脚本；它不能冒充默认的异常 fallback。本轮只完成差异确认，未加入这类开关或 Enumerator 补丁。
