# URGE CRuby C++ Binding Generator

本目录保存由 `core/` 头文件导出的 CRuby 胶水代码。整条链路只读取 `core/*.h`，
不依赖 CRuby VM 私有结构，也不修改任何引擎源码。

```
core/*.h ──gen_api_json.py──▶ bind/api_reference.json ──generate_binding.py──▶ bind/binding_*.{h,cc}
                                                                    └────────▶ bind/binding_init.{h,cc}
bind/api_reference.json ──▶ bind/viewer/   只读查看器（UI + 命令行查询）
bind/cruby_utils.{h,cc}   ← 手写运行时层（不生成）
```

CRuby 解释器来自 `third_party/cruby`（1.9.3），CMake 目标为 `ruby-static`，其
`target_include_directories` 是 `PUBLIC`，所以只要链接它就能 `#include "ruby.h"`。

## 运行方式

```bash
python bind/gen_api_json.py          # core/*.h  → bind/api_reference.json
python bind/generate_binding.py      # IR       → bind/binding_*.{h,cc}
```

两个脚本都可用第一个位置参数覆盖默认路径（`gen_api_json.py <output.json>`、
`generate_binding.py <ir.json> <output_dir>`）。除 `meta.generated_at` 外，重复运行
结果逐字节一致；`.cc` 的重跑是幂等的（见「特殊绑定」）。

## IR 查看（`bind/viewer/`）

脚本作者查 API 用的只读查看器，数据源就是 `api_reference.json`。不参与编译，
`bind/CMakeLists.txt` 只 glob `*.cc`，不会把它带进构建。

```bash
python bind/viewer/viewer.py serve           # 127.0.0.1:8770，实时读 IR 并开浏览器
python bind/viewer/viewer.py embed           # 写 viewer/api_embed.js，供 file:// 双击打开
python bind/viewer/viewer.py show Sprite     # 命令行：一个类的完整 Ruby 面
python bind/viewer/viewer.py search bitmap   # 命令行：按名字/签名/参数/类型检索
python bind/viewer/viewer.py stats            # 汇总计数 + 未能导出项
```

- `index.html` 优先 `fetch("../api_reference.json")`（相对 `viewer/` 上一级）；`file://`
  下 fetch 被禁止，自动退回 `api_embed.js` 快照。页面右上角标出 `live` / `embedded`。
- 呈现规则对 IR 做了三类收敛，规则与 `viewer.py` 一一对应：
  属性读写对合并为一条记录（读取侧显示 `Sprite#bitmap`，写入侧 `Sprite#bitmap = value`）；
  模块属性由 `functions` 里的一对 `attr: true` 条目折回单条（IR 中模块**没有**
  `attributes` 字段，与类不同）；默认值按脚本字面量呈现（`nullptr→nil`、`{}→[]`/`""`、
  `255.f→255.0`），未映射的形状原样保留。
- `serve` 只绑定 `127.0.0.1`，因为它把 `bind/` 暴露成无鉴权的 HTTP。
- 查看器**只读** IR，从不改写 `core/`、`api_reference.json` 或任何生成物。
  详细用法见 `bind/viewer/README.md`。

## 构建

```bash
cmake -S . -B build                                  # 仅当生成文件数量变化时需要
cmake --build build --config Debug --target urge-binding
```

产物 `build/bind/Debug/urge-binding.lib`（静态库）。`bind/CMakeLists.txt` 用
`file(GLOB ...)` 收集 `*.cc`，与 `core/CMakeLists.txt` 一致；CMake 在 configure 时冻结
文件列表，所以**新增/删除** `binding_*.cc` 后必须重跑一次 `cmake -S . -B build`，否则
链接会报 `LNK2019`/`LNK2001`。

`urge-binding` 是纯胶水库，不含入口点。宿主（`app/`）负责 `ruby_init()` /
`ruby_sysinit()` 之后调用 `binding::InitBindings()`；Ruby 与引擎循环的驱动不在本库内。

### 运行时验证（`bind/test/`）

```bash
cmake --build build --config Debug --target urge-binding-smoke
cd build/bind/test/Debug && ./urge-binding-smoke.exe
```

`bind/test/smoke_main.cc` 启动内嵌 VM、调用 `InitBindings()`，然后用一段 Ruby 脚本逐条
断言 API 契约（`failures=0`、退出码 0 表示全部通过）。它不是交付物的一部分，但**改动
导出块、`URGE_BINDING` 注解、生成器覆盖项或 `cruby_utils.h` 之后必须跑一次**：编译器检查不了
`rb_data_type_t` 的布局、`SetupSelfData`/`GetSelfData` 的往返、`ParseArgs` 写入的类型、
类树与 Ruby 名，只有真机跑一遍才能确认。

脚本只用**构造方式**验证不需要 GPU 的类（`Disposable`、`Rect`、`Color`、`Tone`、
`Vector2/3/4`、`Table`、`Font`）；`Bitmap`、`Node`、`Sprite`、`Viewport`、`Plane`、
`Palette` 只做注册级检查（`allocate` 是否给出 typed data、方法名是否齐全、
`initialize`/`initialize_copy` 是否为 `private`），因为实例化它们需要活的图形设备；
`Graphics`/`Input` 的**函数**一律不调用——它们读的单例要等窗口建立后才存在，调用会是空
解引用——只检查模块性、方法名和 `Input::*` 常量。

两个与 Ruby 1.9.3 有关的坑已经写在脚本注释里，改动时不要"修回去"：

- `$stdout.sync = true` 必须设：`main()` 不调用 `ruby_cleanup`，否则 `puts` 的输出留在
  缓冲区里，报告只剩 C 侧的 `printf`。
- `Module#method_defined?` 传 **String 会返回 false**，必须传 Symbol（`respond_to?` 没这
  个问题）。

## 输入与导出范围

- 只扫描 `core/*.h`；`meta.source_headers` 记录全部被扫描文件。
- 只有类体内含 `/*-export.begin-*/` 与 `/*-export.end-*/` 的类进入 IR；两者之间的
  成员函数与属性才导出，其它声明忽略（`Drawable`、`IniFile`、`Config` 等不导出）。
- **构造函数是唯一例外**：从整个类体收集，因为引擎把构造声明写在导出块之外
  （`Node`、`Vector2/3/4`、`Palette`）。参数含内部 C++ 类型（`ZValue`、`glm::vec*`、
  `SDL_Surface*`、`DrawParam`）的构造不导出，只记入 `unsupported`。
- `Singleton<T>` 派生的类（`Graphics`、`Input`，脚本常量 `MODULE_CLASSES`）进入
  `modules`，其余进入 `classes`。
- 每个 C++ 基类都必须有独立的 Ruby 类与 `Init*Binding()`。派生类只注册自己声明的
  成员，**禁止**把父类方法复制注册到派生类；父类 VALUE 必须用
  `rb_const_get(rb_cObject, rb_intern("Parent"))` 取得，不能传 `rb_cObject`。
- `ATTR(type, name)` 展开为 Ruby 的 `name` 与 `name=`。urge 的 `ATTR` 常带内联函数体
  （读写共用一个实现，读路径返回 `std::optional`），胶水统一按 `Attr_<name>(...)`
  调用，不关心它是声明还是内联定义。

当前规模：15 个类 + 2 个模块；跳过 5 个构造：

| 类 | 签名 | 原因 |
| --- | --- | --- |
| `Node` | `Node(RefPtr<Node>, const ZValue&)` | 内部参数类型 `const ZValue&` |
| `Palette` | `Palette(SDL_Surface*)` | 内部参数类型 `SDL_Surface*` |
| `Vector2` | `Vector2(glm::vec2)` | 内部参数类型 `glm::vec2` |
| `Vector3` | `Vector3(glm::vec3)` | 内部参数类型 `glm::vec3` |
| `Vector4` | `Vector4(glm::vec4)` | 内部参数类型 `glm::vec4` |

## 数据类型与分配器（重要）

`RB_DEF_TYPE(Klass)` 生成 `kKlassDataType`（`RB_DATATYPE(Klass, #Klass, ReleaseDataType<urge::Klass>)`），
头文件里用 `RB_DECL_TYPE(Klass)` 声明。每个类在 `rb_define_class` 之后**必须**调用：

```cpp
rb_define_alloc_func(klass, ClassAllocate<&kKlassDataType>);
```

即使 Ruby 侧存在父类，也必须为每个类单独注册自己的分配器：`ClassAllocate<&kParentDataType>`
分配出的对象在 `GetObject<Child>` / `WrapObject` / `_load`（内部走
`rb_check_typeddata(v, &kChildDataType)`）时会被判定为 "wrong argument type"；`_load` 用
`rb_obj_alloc(self)`，类缺少分配器时 Marshal 反序列化必然失败。

`WrapObject` 通过 `type.wrap_struct_name` 反查 `rb_const_get(rb_cObject, ...)`，因此
**Ruby 类名必须与 C++ 类名一致**并注册在顶层。

模块（`Graphics`/`Input`）没有 C++ 实体，其头文件**不**声明 `RB_DECL_TYPE`。

## CRuby 兼容层契约

所有 API 均来自 `bind/cruby_utils.h`：

```cpp
RB_FUNC(Class_method) { /* int argc, VALUE* argv, VALUE self */ }
ParseArgs(argc, argv, "ifso", &integer, &number, &string, &object);
EXC_BEGIN { /* urge 调用 */ }
EXC_END;
return SetupSelfData(self, object.get());
```

`ParseArgs` 格式字符及其写入的 C++ 类型：

| 字符 | 写入类型 | 说明 |
| --- | --- | --- |
| `o` | `VALUE` | 任意对象（`nil` 也是合法 VALUE） |
| `i` | `int32_t` | 用 `int32_t`/`int` 承接 |
| `u` | `uint32_t` | 用 `uint32_t` 承接 |
| `l` | `int64_t` | |
| `p` | `uint64_t` | |
| `s` | `std::string` | 任意可 `to_str` 的对象 |
| `z` | `const char*` | 以 NUL 结尾的 C 字符串 |
| `f` | `double` | |
| `b` | `bool` | |
| `n` | `std::string` | Symbol 或 String |
| `r` | `void*` | 原始缓冲区指针（String 内部指针） |
| `\|` | — | 标记其后的参数为可选 |

注意：`i`/`u` 写入的是 32 位整数，承接变量必须声明为 `int`/`int32_t`（Windows/MSVC 上
`long` 恰好 4 字节，但 LP64 平台是 8 字节，会产生未定义行为），禁止用 `long` 承接。

不得使用 `rb_scan_args`、`mrb`、`MRB_*`、`RB_ARGS_*`、`DefineClass`、
`RB_DATATYPE_*`，也不得手工访问 `argv[i]`。对象通过 `GetObject<T>(value, kTDataType)`
转换，返回对象通过 `WrapObject(ptr, kTDataType)` 转换（`nullptr` 返回 `nil`）。

1.9.3 上没有 `rb_utf8_str_new_cstr` / `rb_enc_str_new_cstr`；返回 `std::string` 用
`rb_enc_str_new(ptr, len, rb_utf8_encoding())`，返回 C 字符串用
`rb_enc_str_new(ptr, std::strlen(ptr), rb_utf8_encoding())`。

### Win32 宏污染（`cruby_utils.h` 顶部的 `#undef`，勿删）

`<windows.h>` 会沿着 `ruby.h` → `third_party/cruby/thread_win32.h` 进入**每一个**胶水
TU，而它把若干短名字定义成宏。函数式宏会静默改写引擎自己的声明，因为 `core/*.h` 是在
`cruby_utils.h` **之后**被解析的：

| 来源 | 宏 | 后果 |
| --- | --- | --- |
| `WinUser.h` | `DrawText` → `DrawTextA` | `core/bitmap.h` 的 `void Bitmap::DrawText(...)` 变成 `Bitmap::DrawTextA`，引擎从未定义该成员 → 每个 TU 都链到 `urge::Bitmap::DrawTextA`，`LNK2019` |
| `wingdi.h` | `ERROR` → `0` | 目前 `core/` 里唯一的 `ERROR` 在字符串字面量中（`core/logger.h` 的 `"[URGE] [ERROR] "`），宏碰不到；但将来任何拼作 `ERROR` 的枚举量会被无声改写 |

因此 `cruby_utils.h` 在**全部 ruby 头之后、第一个 `core/` 头之前** `#undef` 掉这两个宏。
实测方式：编译一个同时包含 `cruby_utils.h` 与 `core/bitmap.h` 的探针，用 `#ifdef` + `#pragma
message` 打印 —— `DrawText` 确实是宏、`ERROR` 确实是宏，而 `min`/`max` 不是（`minwindef.h`
的那一对在本构建里已被中和）。

新增导出方法前，若方法名与 Win32 宏撞名，先机械核对一遍：把 `core/*.h` 的全部标识符与
SDK `um/`、`shared/` 下的 `#define` 取交集。已知可达的冲突只有上表两条。

固定数量参数用 `CheckArgc(argc, N)`：引擎侧方法都以 `-1` 注册（`DefineMethod` 固定传
`-1`），多传参数不会由 VM 报错，必须自己检查。

### 异常桥接

**每个函数体都包在 `EXC_BEGIN`/`EXC_END` 里**，包括只有一个形态的方法。`urge::Exception`
在引擎任意深度抛出（GPU 失败、文件缺失、对象已 dispose），正常化为 Ruby 异常是唯一
安全的做法——让 C++ 异常穿过 VM 的 C 函数是未定义行为。

| `urge::Exception::Type` | Ruby 异常 |
| --- | --- |
| `kExitError` | `SystemExit` |
| `kResetError` | `RGSSReset` |
| `kRGSSError` / `kGPUError` | `RGSSError` |
| `kIOError` | `Errno::ENOENT` |
| 其它 | `StandardError` |

`RGSSReset` / `RGSSError` 由 `InitBindings()` 在仍为 `nil` 时定义（`DefineRGSSExceptions`），
使本库自足；宿主若想指定父类，先于 `InitBindings()` 自行定义即可（变量是
`binding::g_reset_exception` / `binding::g_rgss_exception`）。

## 重载与默认参数

有重载或默认参数时，先按精确的 `argc == N` 分支，再在分支内用只覆盖实际参数的
`ParseArgs`；非法数量统一 `rb_raise(rb_eArgError, "%s", "wrong number of arguments")`。
只有一个形态、且该形态没有默认参数的声明，用 `CheckArgc` + 直接 `ParseArgs`。
禁止 `argc >= N`、通配格式（含对默认参数用 `|`）或手工访问 `argv[i]`。

```cpp
RB_FUNC(Bitmap_Blt) {
  auto* self_obj = GetSelfData<urge::Bitmap>(self);

  EXC_BEGIN {
    if (argc == 4) {
      // blt(x, y, src_bitmap, src_rect)
      int x, y;
      VALUE src_bitmap_val, src_rect_val;
      ParseArgs(argc, argv, "iioo", &x, &y, &src_bitmap_val, &src_rect_val);
      self_obj->Blt(x, y,
          GetObject<urge::Bitmap>(src_bitmap_val, kBitmapDataType),
          GetObject<urge::Rect>(src_rect_val, kRectDataType));
    } else if (argc == 5) {
      // blt(x, y, src_bitmap, src_rect, opacity)
      int x, y, opacity;
      VALUE src_bitmap_val, src_rect_val;
      ParseArgs(argc, argv, "iiooi", &x, &y, &src_bitmap_val, &src_rect_val,
          &opacity);
      self_obj->Blt(x, y,
          GetObject<urge::Bitmap>(src_bitmap_val, kBitmapDataType),
          GetObject<urge::Rect>(src_rect_val, kRectDataType), opacity);
    } else {
      rb_raise(rb_eArgError, "%s", "wrong number of arguments");
    }
  }
  EXC_END;
  return Qnil;
}
```

构造函数与 `initialize_copy` 统一以两参形式返回：

```cpp
return SetupSelfData(self, obj.get());
```

`SetupSelfData` 会 `AddRef()`，所以 `obj`（`RefPtr`）在函数返回时析构是安全的。

`arity_map` 在生成期拒绝歧义：若两个重载在同一 `argc` 上都可调用，脚本直接报错退出，
不会产出「按声明顺序碰运气」的代码。

## 初始化函数

```cpp
void InitGraphicsBinding() {
  auto mod = rb_define_module("Graphics");
  DefineModuleFunction(mod, "update", Graphics_Update);
  ...
}

void InitBitmapBinding() {
  auto parent = rb_const_get(rb_cObject, rb_intern("Disposable"));
  auto klass = rb_define_class("Bitmap", parent);
  rb_define_alloc_func(klass, ClassAllocate<&kBitmapDataType>);

  DefineMethod(klass, "initialize", Bitmap_initialize);          // 实例方法
  DefineClassMethod(klass, "exist", Font_Exist);                 // 类方法
  DefineModuleFunction(mod, "update", Graphics_Update);          // 模块函数
}
```

`InitBindings()` 按继承拓扑顺序调用各 `Init*Binding()`，顺序即 `gen_api_json.py` 的
`CLASS_ORDER`（去掉模块）：

```
Disposable → Node → Bitmap → Color → Font → Palette → Plane → Rect → Sprite
→ Table → Tone → Vector2 → Vector3 → Vector4 → Viewport → Graphics → Input
```

要求 `ruby_init()` 已运行；重复调用会重定义类，因此只调用一次。

## 属性与 Marshal

属性宏按类型选择（前缀 `BINDING_ATTR` 为实例、`BINDING_CLASS_ATTR` 为 `static ATTR`）：

| C++ 属性类型 | 宏 |
| --- | --- |
| 整数（`int8/16/32`、`uint*`、`int64`、`uint64`） | `BINDING_ATTR_INT` |
| `float` / `double` | `BINDING_ATTR_FLOAT` |
| `bool` | `BINDING_ATTR_BOOL` |
| `std::vector<std::string>` | `BINDING_ATTR_STRINGVECTOR` |
| `RefPtr<Obj>` | 见下 |

`RefPtr<Obj>` 按**目标类是否可 Marshal** 选择：

- 目标类在导出块同时声明了 `MARSHAL_DUMP`/`MARSHAL_LOAD` → `BINDING_ATTR_OBJECT`
  （每次读取新建包装对象，因为引擎可能换一个指针出来）
- 否则 → `BINDING_ATTR_OBJECT_REF`（包装对象缓存在 `@_<name>` 实例变量里，重复读取
  返回同一个 Ruby 对象，`equal?` 成立）

urge 中可 Marshal 的是 `Color`、`Rect`、`Tone`、`Table`；因此 `Sprite#src_rect`、
`Sprite#color`、`Sprite#tone`、`Plane#color`、`Plane#tone`、`Viewport#rect`、
`Viewport#color`、`Viewport#tone`、`Font#color`、`Font#out_color` 用 `BINDING_ATTR_OBJECT`，
而 `Bitmap#font`、`Sprite#bitmap`/`#viewport`、`Plane#bitmap`/`#viewport`、
`Viewport#color`(非 `RefPtr`)…等指向引擎持有对象的属性用 `BINDING_ATTR_OBJECT_REF`。
`RefPtr<Vector3>`/`RefPtr<Vector4>`（`Node#position`/`#quaternion`/`#scale`）没有
Marshal 定义，按同一规则走 `BINDING_ATTR_OBJECT_REF`。

导出区同时声明 `MARSHAL_DUMP` 和 `MARSHAL_LOAD` 的类（`Color`、`Rect`、`Table`、`Tone`）
生成 `_dump` 实例方法与 `_load` 类方法：

```cpp
RB_FUNC(Class__dump) {
  auto* self_obj = GetSelfData<urge::Class>(self);

  EXC_BEGIN {
    std::string result =
        urge::Class::MarshalDump(urge::RefPtr<urge::Class>(self_obj));
    return rb_str_new(result.data(), static_cast<long>(result.size()));
  }
  EXC_END;
  return Qnil;
}

RB_FUNC(Class__load) {
  std::string data;
  ParseArgs(argc, argv, "s", &data);

  EXC_BEGIN {
    VALUE obj = rb_obj_alloc(self);
    auto ptr = urge::Class::MarshalLoad(data);
    return SetupSelfData(obj, ptr.get());
  }
  EXC_END;
  return Qnil;
}
```

`_dump` 用 `DefineMethod(klass, "_dump", ...)`、`_load` 用
`DefineClassMethod(klass, "_load", ...)` 注册。

### 模块属性

模块属性（`Graphics.frame_rate` / `frame_count` / `brightness`）走单例：
读 `urge::Graphics::Get().Attr_FrameRate()`，写 `Attr_FrameRate(value)`。读取要解引用
`std::optional`（`(*...())`），写法与实例属性宏一致；两个方向都注册成 module function
（`frame_rate` / `frame_rate=`）。

## 拷贝构造

C++ 声明拷贝构造（`Class(RefPtr<Class>)`）时绑定为 `initialize_copy`（Ruby 的
`dup`/`clone`）：

```cpp
RB_FUNC(Class_initialize_copy) {
  VALUE other;
  ParseArgs(argc, argv, "o", &other);

  urge::RefPtr<urge::Class> obj = nullptr;
  EXC_BEGIN {
    auto other_obj = GetObject<urge::Class>(other, kClassDataType);
    obj = urge::MakeRefCounted<urge::Class>(other_obj);
  }
  EXC_END;

  return SetupSelfData(self, obj.get());
}
```

适用于 `Bitmap`、`Rect`、`Color`、`Tone`、`Vector2/3/4`、`Font`、`Table`。

## 模块绑定

`Graphics`、`Input` 是 `Singleton` 类，不定义 Ruby 类而是模块，方法通过
`DefineModuleFunction` 注册，函数体用 `urge::X::Get()` 取单例。这两个头文件不声明
`RB_DECL_TYPE`：

```cpp
RB_FUNC(Input_Update) {
  EXC_BEGIN {
    CheckArgc(argc, 0);
    urge::Input::Get().Update();
  }
  EXC_END;
  return Qnil;
}
```

## `URGE_BINDING` 注解

```cpp
URGE_BINDING(Name : "[]")
int16_t Get(int32_t x, int32_t y = 0, int32_t z = 0);
```

- 宏本身在 `core/definition.h` 里展开为空，只在扫描期有意义。
- 语义：**只作用于紧跟其后的那一条声明**。
- 目前只识别 `Name : "..."` 一个键，用来把 C++ 名映射成驼峰→下划线规则得不到的
  Ruby 名（`Table#[]` / `#[]=`）。
- 注解出现在导出块里才生效；带注解的声明仍走常规的签名分析（重载、默认参数照常）。
- `Name` 缺省时注解无效果，声明按默认命名规则处理。

## 特殊绑定：生成器覆盖项 + 手写区

签名描述不了的 Ruby 侧行为集中放在 `generate_binding.py` 里，而不是让每次生成
都去猜：

| 覆盖项 | 内容 |
| --- | --- |
| `TABLE_MANUAL` | `Table#[]` / `#[]=` 的手写实现（写进 `binding_table.cc` 的手写区） |
| `INPUT_MANUAL` | `Input::*` 键常量（写进 `binding_input.cc` 的手写区） |
| `EXTRA_INIT_CALLS` | 追加到 `Init*Binding()` 末尾的语句（`Input` → `DefineInputKeyConstants(mod);`） |
| `HANDWRITTEN_SEED` | 新文件首次生成时播种的手写区内容 |

命名**不在**这里 —— 它只有「注解 + snake_case」两条规则，见下节。

### 生成物分块

每个 `.cc` 固定由四个块组成，重跑时只替换两块 `GENERATED`，另外两块逐字保留：

```cpp
// --- GENERATED INCLUDES BEGIN ---   ← 每次重写
// --- GENERATED INCLUDES END ---
// --- HANDWRITTEN INCLUDES BEGIN --- ← 逐字保留
// --- HANDWRITTEN INCLUDES END ---
namespace binding {
// --- HANDWRITTEN BEGIN ---          ← 逐字保留
// --- HANDWRITTEN END ---
// --- GENERATED BEGIN ---            ← 每次重写
// --- GENERATED END ---
}  // namespace binding
```

`binding_*.h` 每次整体重写（无手写区）；`binding_init.{h,cc}` 同规则。
因此「手写实现 + 生成注册」是安全的：手写区在文件里位于生成区之前，生成区引用它
（如 `InitTableBinding()` 里的 `Table_Get`）时符号已被声明。**不要**把需要跨文件使用
的东西放手动区——手写内容只出现在它自己的翻译单元里，跨文件请加进 `*_INCLUDES` 区
（同样逐字保留）或提到 `cruby_utils.h`。

### 当前手写项

`Table#[]` / `#[]=`：Ruby 按 `(x[, y[, z]], value)` 传参，而 `Table::Set` 的签名是
`Set(value, x, y, z)`，参数需要重排；越界读返回 `nil`、越界写忽略。`Table::Get` 返回
`int16_t`，用 `INT2NUM`。

`Input` 的键常量表在 `core/input.h` 的 `urge::kKeyboardBindings` 里，不从导出块声明，
因此用 `DefineInputKeyConstants(mod)` 遍历注册。

## 命名映射规则

只有两条，优先级从高到低：

1. **头文件注解** `URGE_BINDING(Name : "...")`，见上一节。机械规则给不出的名字
   一律在这里写明。
2. **`camel_to_snake`**，其余全部按它解析。缩写按常规处理：
   `StretchBlt`→`stretch_blt`、`DrawText`→`draw_text`、`OX`→`ox`、`BGMPlay`→`bgm_play`。

**没有覆盖表。** `gen_api_json.py` 里原有的 `METHOD_NAME_OVERRIDES` /
`ATTR_NAME_OVERRIDES` 已删除：**C++ 名怎么拼，Ruby 名就怎么来**。想让 Ruby 名好看就
直接把 C++ 名改成那样（`GetWidth` → `Width`、`XSize` → `Xsize`、`FadeIn` → `Fadein`）；
改不了、或机械规则给不出的（`[]`、`exist?`、`press?`、`rect`）才写注解。
少一层间接的好处是重命名时不会留下对不上的表项 —— 删表前后 36 个生成物逐字节一致，
就是因为那些表项早已被注解遮蔽。

结构槽位与上面两条并列，不是"函数命名"：

- 构造函数 → `initialize`、拷贝构造 → `initialize_copy`；
- `MARSHAL_DUMP`/`MARSHAL_LOAD` → `_dump`/`_load`；
- `ATTR(type, name)` → `name` 与 `name=`（写入侧 `name=`）。

### 现行名字一览

| C++ 名 | Ruby 名 | 依据 |
| --- | --- | --- |
| `Bitmap::Width` / `Graphics::Width` / `Sprite::Width` | `width` | `camel_to_snake` |
| 同上 `Height` | `height` | `camel_to_snake` |
| `Graphics::Fadein` / `Fadeout` | `fadein` / `fadeout` | `camel_to_snake` |
| `Table::Xsize`/`Ysize`/`Zsize` | `xsize`/`ysize`/`zsize` | `camel_to_snake` |
| `Bitmap::GetPixel` / `SetPixel` | `get_pixel` / `set_pixel` | `camel_to_snake` |
| `Input::GetKeyName` | `get_key_name` | `camel_to_snake` |
| `Font::Existed` | `exist?` | 注解 |
| `Bitmap::GetRect` | `rect` | 注解 |
| `Disposable::IsDisposed` | `disposed?` | 注解 |
| `Input::Pressed`/`Triggered`/`Repeated` | `press?`/`trigger?`/`repeat?` | 注解 |
| `Input::KeyPressed`/`KeyTriggered`/`KeyRepeated` | `key_press?`/`key_trigger?`/`key_repeat?` | 注解 |
| `Table::Get`/`Set` | `[]`/`[]=` | 注解 |

注意 `IsDisposed` 的机械结果是 `is_disposed`、`KeyPressed` 是 `key_pressed` ——
它们之所以是 `disposed?` / `key_press?`，**只因为头文件上有注解**。注解是唯一生效
路径，删掉它名字就会退回机械结果。

`camel_to_snake` 在 `gen_api_json.py` 与 `generate_binding.py`（后者只用于生成文件名
`binding_<name>.cc`）各有一份实现，已用 121 个名字比对等价；改一个务必同步另一个。

新导出 `Input::GetKeyName` 属于默认规则的结果，即 `get_key_name`（未加注解）。

## IR schema（`bind/api_reference.json`）

```json
{
  "meta":    { "title", "generator", "rules", "generated_at", "source_headers", "skipped" },
  "modules": [ /* Graphics / Input */ ],
  "classes": [ /* 15 个导出类，按 CLASS_ORDER */ ],
  "aliases": []
}
```

class / module 条目字段：

| 字段 | 含义 |
| --- | --- |
| `kind` | `class` / `module` |
| `cpp_name` | C++ 类名，同时是 Ruby 类名 |
| `cpp_parent` | C++ 基类（模板参数已剥离，如 `Singleton`） |
| `ruby_superclass` | `rb_define_class` 第二参数（`Disposable`/`Node`/`Object`）；模块为 `null` |
| `header` / `export` | 来源头文件与导出块行区间 `core/x.h:beg-end` |
| `constructors` | `initialize` 每个重载的参数（`name`/`type`/`default`） |
| `initialize_copy` | 拷贝构造 `Class(RefPtr<Class>)` → `initialize_copy` |
| `instance_methods` / `class_methods` | 实例/类方法（`static`），含 `return` 与 `params` |
| `attributes` / `class_attributes` | `ATTR(...)` / `static ATTR(...)` → 读写对 |
| `data_attributes` | 公开数据成员按同一规则生成读写对（urge 的导出块里没有这类声明） |
| `marshal` | `{dump, load}`，两者都为 `true` 才生成 `_dump`/`_load` |
| `index` | `URGE_BINDING(Name:)` 标出的 `{get, set}`（`Table#[]`/`#[]=`） |
| `functions` | 模块函数；属性读写对是两条 `attr: true` 条目，带 `setter` 与 `attr_value_type` |
| `unsupported` | 被跳过的声明及原因（内部参数类型等） |
| `export` | 见上 |

`gen_api_json.py` **只读**头文件，不修改任何文件。

## 生成后检查

生成后必须检查：

- 每个导出声明都有对应实现和注册；每个重载/默认参数的数量分支完整。
- 每个类（含叶子类）都注册了 `rb_define_alloc_func`。
- 属性宏选择符合上文规则；`_dump`/`_load` 已为 `Color`/`Rect`/`Table`/`Tone` 生成。
- 对比每个 `binding_*.cc` 中 `DefineMethod`/`DefineClassMethod`/`DefineModuleFunction`
  的注册名，确保全部出现在 JSON 中；任何注册名缺失或多余都说明扫描规则或命名映射
  需要同步更新。
- 生成物行宽 ≤ 80，且保持 CRLF（两个生成脚本都以 `newline="\r\n"` 写文件）。
  `bind/` 里唯一的例外是 `viewer/api_embed.js`，`viewer.py` 以 `newline="\n"` 写它；
  手写文件（`Bindgen.md`、`cruby_utils.*`、`*.py`、`test/*`）本来就是 LF，别去"统一"。
- 绑定目标 `urge-binding` 能编译（零错误、零告警）。
- `urge-binding-smoke` 跑过且 `failures=0`，见「运行时验证」。
- 用 `python bind/viewer/viewer.py stats` 复核一遍计数：`unsupported` 数量应与
  `meta.skipped` 一致，`module attribute` 数量应为模块属性读写对的一半。

生成器不得修改 `core/`、公共 API 或无关文件。

## 修改流程

改动任一导出声明时须同步：

1. 更新 `core/*.h` 的导出块（必要时加 `URGE_BINDING(Name:)`）；
2. **若改了 C++ 方法名，先改完 `core/` 和 `app/` 里的全部调用点**——绑定依赖
   `urge-core`，`core/` 编不过就看不到绑定自己的错误。快速自检：
   `grep -rn 'OldName' core/ app/` 必须为空；
3. 重新运行 `python bind/gen_api_json.py`；
4. 重新运行 `python bind/generate_binding.py`；
5. **手写区不会跟着重命名走**。`binding_table.cc` / `binding_input.cc` 的
   HANDWRITTEN 块逐字保留，所以只要手写块调用过被改名的方法，就必须同时改
   `generate_binding.py` 的 `HANDWRITTEN_SEED` **和**已存在生成物里的那一块；
   更稳的做法是删掉该 `.cc` 再跑一次生成器（文件缺失时它用种子重建）。
   自检：`grep -c 'OldName' bind/binding_*.cc` 必须为 0；
6. 若 Ruby 可见名变了（新增、改名、加 `?`），同步 `bind/test/smoke_main.cc`
   里对应的断言——它按名字断言，改名前它不会失败，改名后才会；
7. 编译 `urge-binding` 与 `urge-binding-smoke`，跑一次冒烟测试，并做一次上文
   「生成后检查」；
8. 重跑 `python bind/viewer/viewer.py embed`，否则 `file://` 打开的查看器
   还在用旧快照。

签名描述不了的新行为（参数重排、额外常量、非常规注册）加进 `generate_binding.py`
的覆盖项，**不要**直接改生成物——GENERATED 块会被下一次运行覆盖。名字不在此列：
改 C++ 名或加一条 `URGE_BINDING` 注解即可，没有需要同步的命名表。

## 与 lime 的差异

以 `rgssproj/lime/binding/core/` 为参考移植，已知差异：

| 方面 | lime | urge |
| --- | --- | --- |
| 命名空间 | `lime::` | `urge::` |
| 单例取值 | `lime::X::Instance()` | `urge::X::Get()` |
| 属性声明 | 裸 `ATTR(ty, name);` | `ATTR(ty, name) { ... }` 带内联函数体，常带 `virtual` |
| `RB_DEF_TYPE` 释放 | `ReleaseDataType<lime::Klass>` | 同（`urge::RefCounted::Release`） |
| 异常类型 | 独立枚举 | `urge::Exception::Type`，多了 `kGPUError`（并入 `RGSSError`） |
| 导出语法 | 无注解 | `URGE_BINDING(Name : "...")`，只作用于下一条声明 |
| 命名 | 覆盖表（脚本内的旧名 → Ruby 名映射） | **无覆盖表**：`camel_to_snake` + 注解，C++ 名直接决定 Ruby 名 |
| 属性宏族 | 无 STRINGVECTOR / CLASS_ATTR_* | 新增 `BINDING_ATTR_STRINGVECTOR`、`BINDING_CLASS_ATTR_{INT,FLOAT,BOOL,STRINGVECTOR,OBJECT}` |
| 参数检查 | 固定数量方法不检查 | 统一 `CheckArgc`（方法以 `-1` 注册） |
| 异常包裹 | 视情况 | 每个函数体统一 `EXC_BEGIN`/`EXC_END` |
| 类集合 | `ViewportChild`/`Effect`/`Tilemap`/`Window` 等 | `Node` 为通用 3D 节点基类，无 `ViewportChild`/`Effect`/`Tilemap` |
| 入口点 | `binding/cruby_main.cc` 自带 `main` | 本目录只产静态库，入口点归宿主 |
| CRuby 版本 | `3rdparty/ruby-193` | `third_party/cruby`（同为 1.9.3，头文件逐字节一致） |
