# 对象 GC 与脚本绑定生命周期

## 目标与边界

- core 不依赖 Ruby 头文件、`VALUE` 或脚本 API。
- 纯 C++ 创建的托管对象不需要脚本 wrapper。
- 已暴露的原生对象复用原脚本 wrapper，保留实例变量、Ruby 子类、单例方法与冻结状态。
- 原生强引用保留目标的原 wrapper；不可达的原生／脚本循环不成为永久根。
- GC 管理对象存活，不替代 GPU、音视频和网络资源的显式关闭协议。

core 的实现位于 `core/common/garbage_collector.{h,cc}`、`member.h` 与 `persistent.h`；CRuby 协调及身份表只位于 `bind/native_gc.{h,cc}`。

## C++ 指针与分配

| 类型 | 含义 | 放置位置 |
|---|---|---|
| `T*`、`MaybePtr<T>` | 已登记的 GC 对象地址，本身不保活；后者表达可空 API | 公开 C++ 参数、返回值、临时借用 |
| `Member<T>` | 不计根的强引用边；复制、清空不删除对象 | 托管对象及其拥有的结构／容器 |
| `WeakMember<T>` | 不保活目标；目标回收前自动失效 | 父回指、观察关系、非拥有注册 |
| `Persistent<T>` | 有类型的显式外部／临时 GC 根 | C++ 局部、非托管服务、跨调用任务 |
| `GarbageCollector::Root` | 无类型的显式 GC 根，可作为 heap box 跨宿主边界转移 | 绑定转换保护、外部根容器 |
| `std::unique_ptr<T>`、`std::shared_ptr<T>` | 仅管理非 GC 辅助资源的所有权 | 原生句柄包装、backend/helper、非托管控制结构 |

`Object` 继承 `GarbageCollected`，只有收集器决定已登记托管对象何时删除。这是删除旧指针和引用计数体系，不是将 `RefPtr` 改名；没有 `RefCounted`、`AddRef`、`Release` 或兼容分配工厂。标准库共享指针可以在非 GC 辅助资源之间共享所有权，但不能代替 GC 根、管理已登记对象或承载托管强边。

根登记的是句柄自身的地址，不是对象上的引用计数。复制 Root／Persistent 新增独立地址登记；移动将登记转移到新地址并清空源句柄，vector 重分配也遵守此协议。`get()` 仅借用对象地址；`reset()` 和析构只撤销登记，不运行目标析构。没有将根释放为 raw owning pointer 的操作。

```cpp
// 公开 API 和工厂返回 raw pointer，不隐式留下根。
MyObject* CreateObject() {
  return MakeGarbageCollected<MyObject>(args...);
}

Persistent<MyObject> object = CreateObject();
Persistent<MyObject> transferred(std::move(object));
// object 已空；transferred 在其作用域内保护目标及 Trace 的强边。
```

工厂只返回 `T*`，`auto object = MakeGarbageCollected<T>()` 不保留旧代码的根语义。需要跨分配、绑定转换或收集安全点保存对象时，必须立即建立 `Persistent<T>`／Root，或将地址放入已保活对象的 `Member<T>` 字段。不要把 raw 指针或局部 `Member` 误认为根；公开 raw 返回值也不向调用者转移删除责任。

工厂在完整构造并建立临时 Root 之后登记对象；`ConstructionScope` 保护尚未登记的父对象所持有的构造中间对象，其析构不触发延期收集。不得使用未登记的 `new` 绕过该协议。构造失败会清空针对未登记对象的外部 Root／弱槽；登记后的回调若已发布对象再抛异常，仍由正常 GC 决定其寿命，而不是工厂立即删除。

## Trace 约定

派生类须调用父类的 `Trace`，并枚举全部持久强边，包括 map、vector、嵌套 State/Item 等结构。叶子对象可以继承 `Object` 的空 Trace。

```cpp
void MyObject::Trace(GCVisitor& visitor) const {
  Base::Trace(visitor);
  visitor.Trace(texture_);
  visitor.Trace(children_);
}
```

- 不能在 Trace 中创建 `Persistent`／Root 临时根，也不能调用会返回根句柄的 getter 来代替直接字段访问；`GCVisitor::Trace(Persistent)` 被显式禁止。
- Trace 可能重复执行；不得修改图、依赖一次性副作用或调用脚本。
- 不因 `disposed?` 就跳过 Trace：兼容接口可能仍允许读取 disposed 对象的属性。
- `std::function` 捕获的指针不会自动被追踪；非拥有回调需要显式解绑。
- 托管对象所拥有的强边容器元素应是 `Member`，不能使用对象内的独立 `Persistent`／Root 根把循环永久保活。外部根容器则可以保存 Persistent／Root。

现有所有权方向保持不变：2D 节点强持有 parent，DrawableSet 只是非保活绘制注册；3D 节点强持有 children，parent 是弱引用。

## 回收与资源关闭

一批垃圾依次经过：

1. 清空所有指向该批对象的弱槽。
2. 调用全部对象的 `PrepareForCollection()`。
3. 析构全部对象，顺序不保证。

跨对象解绑应在 Prepare 完成。析构不能沿 managed 指针访问其它垃圾对象。Prepare 不得复活垃圾或发布指向垃圾的新边。

`Disposable::PrepareForCollection` 运行幂等 Dispose。2D 节点还需完整摘链和移除回调；以上清理对已经 Dispose 的对象也必须完成。helper 被替换或清空时，先显式 Dispose/Detach：Member 清空不会立即析构并从绘制列表移除它。

清理、关闭资源或 join worker 时不能持 GC 注册表锁。关闭引擎时，先完成脚本退出逻辑，再清除绑定关系；释放原生任务和屏幕根后，在 Font/GPU/Uniform/Quad 后端仍存活时收集剩余托管对象。

## 脚本身份与联合扫描

binding 保存地址稳定的弱 wrapper capsule，身份表按原生对象地址查找。已有有效记录直接返回原 Ruby 对象；首次暴露才创建 wrapper，并选择原生最派生的已导出类型。

wrapper payload 不持有永久 Persistent／Root。原生和脚本互相可达时，以联合扫描而不是永久强注册避免循环泄漏。

- wrapper 的 dmark 从对应原生对象出发。
- 一个非空 payload 的根 sentinel 扫描原生外部根。
- binding 的 GCVisitor 遍历原生强边，并标记已存在的 wrapper。
- 未暴露的原生对象只继续 C++ Trace，不制造 wrapper。
- `GC_ENTER` slice 重置共享 visited；WB-unprotected datatype 支持 minor 和 incremental 最后重扫。
- 弱 Ruby 槽清为 `Qundef`，不是 `Qnil`；禁止返回 zombie。
- capsule 地址在弱登记期间必须稳定；dfree 只退役，安全边界后才释放记录。
- compaction 修复保存在 binding 中的 VALUE；首版的 mark-and-pin 不等于原生对象移动。

普通自动 Ruby GC 执行宿主图保活，但不能据此清扫整个 native heap。原生清扫只消费已确认完整联合扫描的 epoch。driver 用 START/END_MARK/END_SWEEP 配对；未完成、重入、构造屏障或过期结果都不得授权清扫。Ruby internal hook 中不调用 Ruby API、不析构资源。

原生 collector 是 stop-the-world 模型。不能让 native marking 跨 Ruby mutator 间隔；Member 没有跨堆增量写屏障。`TraceRoots(visitor)` 只提供通用根快照，不改变 native epoch，调用方负责扫描期间的图和根稳定。

异步网络队列保留明确的 `Persistent<NetworkEvent>` 任务根；叶子事件的任务存活由 backend 关闭时撤销任务根。后台分配必须用调用端的外层 `ConstructionScope` 覆盖 raw 工厂调用、外部根登记与返回根移交；工厂自己的屏障在返回时已经撤销，不能保护调用端尚未登记根的窗口。curl 的 `MakeEvent` 遵守此交接协议。

Root／Persistent 的登记、转移与注销受注册表锁保护，允许 worker 持有和转移根，但锁不是跨线程 GC 写屏障：完整标记／清扫及根快照遍历期间，所有图、根和弱槽 mutator 仍须停止。不得将跨 worker 队列改成无安全点协议的 Member。扩展后台强图修改之前，需要独立的线程／分配／停止协议。

## Ruby 转换与异常

Ruby 的非局部退出不运行普通 C++ 栈析构。绑定分为 Ruby 校验、纯 C++ 调用、Ruby 包装三段：

- Ruby 阶段建立私有、稳定的字符串和数值快照，完成可能警告、raise、throw 或再入的转换。
- C++ 阶段只读取快照；不调用 Ruby 数值转换。
- GC 对象返回值先建立 heap-allocated `GarbageCollector::Root`；Ruby 栈只携带 POD 的 `Root*`，不能让需析构的栈上 Persistent／Root 跨 Ruby API 调用。`OwnedSetupSelf`／`OwnedWrapObject` 消费根盒子，`rb_ensure` 在正常、raise、throw 等所有路径精确删除盒子一次，撤销登记而不删除 GC 对象。
- aggregate 盒子同样由 owned helper 的 `rb_ensure` 管理；Node3D children 的 heap snapshot 保存每个子对象的 Root，保护整个数组及 wrapper 转换期间的生命期。
- Marshal 的 wrapper 分配也位于保护区，不能先 `rb_obj_alloc` 再建立清理保护。
- 原引用属性的冻结检查必须显式保留，不能依靠已删除的 ivar 生命周期缓存。
- dup/Marshal 有意创建新原生对象；普通 getter 复用 wrapper。setter 的值复制与克隆语义不因 GC 迁移而变化。

## 退出控制边界

退出请求只在已返回 native owning scope 的绑定边界或校准过的 CRuby 安全帧接力。每个执行作用域只投递一次私有 catch token，保证 unwind 中的 ensure 仍能 dispose/save。

标准 Ruby Fiber 初始化及 resume/transfer、ObjectSpace finalizer 使用绑定层的局部作用域保护；scope registry 是弱的，不能因此永久保活已废弃的 Fiber。包裹帧会改变 backtrace／反射可见形态，但参数、block、kwargs 和普通异常仍按原接口转发。

不承诺从绕过标准入口的直接 C API Fiber/finalizer，或永不返回的未知 native callback 的 Ruby-only 循环中强行退出。未知 owning frame 不允许跨 C++ RAII 非局部跳转；不得以释放活栈或截断 ensure 来掩盖该限制。脚本若在 ensure 中主动替换退出控制流并继续捕获，也不以反复投递破坏其清理过程。

## 验证入口

独立原生测试不链接脚本引擎或 renderer：

```sh
cmake -S test/gc -B build/gc-regression -G "Visual Studio 18 2026" -A x64
cmake --build build/gc-regression --config Debug
ctest --test-dir build/gc-regression -C Debug --output-on-failure
```

完整引擎验证使用 `URGE_BUILD_GC_TESTS=ON`。仅该测试构建暴露 native/capsule 数量及显式收集探针，生产构建不暴露。

```sh
cmake --preset win64-vendored-ruby \
  -DURGE_BUILD_GC_TESTS=ON \
  -DURGE_BINDING_IR_FILE=build/gc-validation-api.json
cmake --build --preset win64-vendored-ruby-debug
python -I test/gc-integration/run.py \
  --engine build/win64-vendored-ruby/app/Debug/Game.exe --rgss 1 2 3
```

`URGE_BINDING_IR_FILE` 允许验证构建使用独立 IR，避免覆盖工作区中已编辑的 API 文档。`URGE_BINDING_GLUE_DIR` 可指定新的生成目录，验证不依赖工作区已有 HANDWRITTEN 的冷构建。生成器同时维护 HANDWRITTEN 的初始种子与现有文件，包括 disposed 兼容、Tilemap 代理、Input 查询和 Mouse 常量；空目录生成不能依赖已有输出掩盖错误。共享生成 stamp 的同一 checkout 应串行生成，不能同时重写不同配置的输出。

`test/gc/garbage_collector_test.cc` 覆盖 raw 工厂无隐式根、Root／Persistent copy/move/reset、根地址随 vector 重分配迁移、late-root、构造异常注销及暂停 worker 根的安全点；可控线程调度还验证 raw 返回到外部根登记的交接屏障阻止新 epoch 和已完成 epoch 的清扫。保留循环、epoch、Sweep、弱槽、cancel 和独立 host 联合扫描回归。可用 `URGE_GC_NO_EXCEPTIONS=ON` 独立配置验证禁用 C++ 异常的契约；ASan 配置只需对该测试目标设置编译／链接选项，不必配置完整引擎。

```sh
python -I test/gc-integration/generation_test.py
```

生成单元测试先将当前 header 的 API 写入 `build/` 下独立 IR，再逐例复制；不会覆盖用户的 `bind/api_reference.json`，也不读取现有 autogen 作为冷构建种子。除 raw pointer／heap Root guard 外，仍验证冷目录、单文件／全目录删除、HANDWRITTEN 保留、字节／时间戳幂等和公共兼容 API。

`test/gc-integration` 包含实际引擎的身份、渲染、coercion、循环、Fiber 语义及悬停关闭回归，Ruby API 未因 C++ 指针迁移而改名；`network_roots.cc` 验证 Persistent 根跨 Network 服务关闭及析构的存活；`test/gc/lazy_sweep_probe.rb` 检查 stress=2、大堆、GC 设置恢复及物理回收。独立原生／Python 单元测试通过不代表这些完整引擎回归已经运行。

Debug CRuby 会保守扫描仍运行的解释器 C 帧。仅让 Ruby 方法返回，不能保证旧 VALUE 消失；循环回收测试应在已经结束的 Ruby Fiber 中构造对象。另外 WeakMap 物理 size 可能滞后，应检查有效 keys。不能用测试栈误保活推导永久泄漏，也不能只以 wrapper 消失证明 native/capsule 已清理。
