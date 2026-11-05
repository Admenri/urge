# Scene3D 管线审计与执行约定

本目录的管线分层已具备 SRP 的基本形态：`World3D` 提供场景、相机和最终目标，
`RenderQueue3D` 收集候选绘制项，`RenderGraph3D` 排序并执行 Pass，
`RenderContext3D` 管理编码器和每个 Pass 的状态，`MaterialPass3D` 缓存 GPU 管线与绑定。
`ForwardPipeline3D` 是预设图，包含阴影、不透明、透明背面和透明正面阶段。

当前 RenderGraph 是持久命名资源上的执行调度器。资源仍由图持有，没有暂态资源别名、
跨帧历史资源导入或生命周期分析。审计范围主要是这条执行链及其使用的相机、节点和包围球；
动画系统与 glTF 解析没有在本轮进行全面审计。

## 本轮修复

| 问题 | 影响 | 修复 |
| --- | --- | --- |
| View/Globals 固定 16 槽循环 | 后续 Pass 使用早期相机；更新 Globals 会覆盖尚未提交的早期绘制 | View 按块扩容，每个 Pass 使用独立区域，Globals 独立分配；对齐来自设备限制 |
| 依赖不完整、环路静默回退 | 先读后写、禁用生产者仍被视为有效、读取错误版本 | 只调度启用的 Pass，建立 RAW/WAR/WAW 边，稳定拓扑排序；缺失生产者和环路报错 |
| `BindTexture` 不参与依赖、Raster/Compute 忽略绑定 | 仅绑定而未填 `reads` 时顺序错误；Compute 未收到图纹理 | Raster/Fullscreen 绑定自动产生读取依赖；Raster 绑定应用到筛选的材质，Compute 绑定解析为目标 |
| MRT 另建纹理、缓存只按名字/尺寸 | 单输出覆写与 MRT 读取使用不同纹理；缩放、格式更新后采样旧资源 | MRT 组合命名资源的视图，不再另建纹理；验证尺寸、采样数和附件重复 |
| Fullscreen 只使用首个输出 | 图声称写入多个输出，实际没有写入 | Fullscreen 和 Raster 使用相同的 MRT 目标解析 |
| Shadow 输出覆盖普通纹理命名空间或声明多个输出 | 图依赖与实际资源类型/写入不一致 | 拒绝同名普通纹理、最终输出别名及多个 Shadow 输出；拒绝未知 Pass kind |
| 队列在初始相机下提前丢弃物体 | Pass/Graph 相机覆写后漏画；阴影漏掉相机层外的投射者 | 保留候选项，按当前 Pass 相机、目标尺寸和层重新裁剪 |
| 相机状态和剔除状态沿用上一 Pass | 无覆写的 Pass 继承之前的相机或 cull 状态 | 每个 Pass 从图/场景默认相机建立状态，Fullscreen 和 Shadow 显式设置剔除/排序 |
| 阴影状态跨帧残留 | 禁用阴影或隐藏灯光后继续采样旧阴影 | 帧开始清空阴影状态；没有有效方向光时跳过，不再使用相机替代灯光 |
| 点光靠位置匹配；方向阴影作用于所有方向光 | 同位置点光认错；未投影灯光被另一盏灯的阴影压暗 | 按灯光对象身份匹配，只选择上传范围内的灯光，并记录方向阴影的灯光索引 |
| 材质绑定仅对 Fragment 可见，阴影布局遗漏 Material/Globals | 顶点纹理采样或材质驱动的顶点位移无法绘制/投影 | 两个阶段可访问材质绑定；阴影管线保留 Material/Globals 并绑定相应资源 |
| `set_fullscreen` 未失效 Builder | 已创建材质切到 Fullscreen 后仍保留网格顶点布局 | 同时失效 Builder 和管线缓存 |
| 设置 Bitmap 被旧 TextureView 遮蔽 | 纹理设置未生效；重复包装同一视图导致频繁重建 | Bitmap 设置清除视图覆写，同一原生视图不触发重建 |
| `SetBuffer` 清空所有 Storage Buffer | 更新一个输入会重置 Compute 的其他输出/累积状态 | 只失效被设置的 Buffer；按反射尺寸分配；补齐比较采样器与空资源检查 |
| MSAA 深度创建 Array View，默认颜色视图覆盖所有 mip | WebGPU 验证失败 | 多采样深度只创建 2D 视图；附件视图只引用一个 mip；验证采样数、resolve 格式和组合 |
| Storage/RenderAttachment 格式能力混淆 | 未启用可选特性就创建不受支持的资源 | 收紧 Storage 格式列表，拒绝当前设备不能渲染的格式和不支持 resolve 的格式 |
| 异常被 World 丢弃，失败后继续编码 | 黑屏无原因，可能提交错误绑定 | 传播 ExceptionState；错误后停止后续 Pass、关闭当前 Pass；World 日志去重并支持恢复 |
| Pass 的 owner 裸指针未解绑 | Pass 被外部保留而图销毁后可能访问悬空指针 | 删除、清空和析构时解绑 owner/context，清空阴影资源 |
| 相机近裁剪面使用 OpenGL 公式 | 与 WebGPU 的 `[0, 1]` 深度裁剪不一致 | 使用矩阵第 2 行提取近平面；保持 far > near；阴影分割受相机 far 限制，正交级联使用正交尺寸 |
| 实例包围球包含关系合并错误 | 更大包围球包含旧球时仍算出过小半径，导致错误裁剪 | 直接采用包含旧球的新球 |
| 节点允许自引用/祖先环；UniformPool 扩容失败后索引越界 | 递归栈溢出、引用环或越界访问 | 拒绝节点环，重设父级时持有自身引用；UniformPool 扩容失败返回无效槽，对齐取整 |

## 使用约定与兼容性

- `screen`、`output`、空名字指向同一个最终输出，不能用来声明普通纹理。
- `colors` 声明输出。Raster/Fullscreen 的 `bind_texture` 自动声明读取；额外读取用 `reads`。
  Compute 的绑定可能是读或写，必须用 `reads`/`colors` 声明实际访问，不能只依赖绑定推断。
- 读取选择声明顺序中最近的前置写入；若没有前置写入，选择首个后置生产者。
  下一次写入必须等这个版本的读取结束。因此调换 Pass 声明顺序会改变读取的资源版本。
- 暂态图资源不能在没有启用生产者的情况下读取。不允许 Raster/Fullscreen 采样自己的附件；
  后处理使用不同输入/输出纹理。以前靠静默回退工作的错误图现在会记录错误并停止当帧图执行。
- MRT 各颜色附件尺寸和采样数必须一致，可包含一个显式深度纹理；没有显式深度时共享首个颜色目标的深度。
  后续单输出 Pass 使用同名目标时共享该目标的颜色与深度。多采样颜色采样使用 resolve 视图。
- 资源声明/缩放变化会清空图的普通目标缓存。通过图绑定的资源会重新解析；外部持有旧目标不会自动变成新目标。
- `SetBuffer` 设置指定 Buffer 的初值，其他 Buffer 的 GPU 状态保留；重复设置同一 Buffer 会重新初始化它。
- `BeginPass` 前配置相机和视图状态。帧内每个 Pass 的 View/Globals 是独立快照。
  原生 Pass/Encoder 包装对象只能在对应的编码作用域内使用。
- 管线仍是单线程执行。必须在前一帧已提交后调用 `BeginFrame`；在同一未提交编码批次中重置上下文，
  或多次修改同一材质/Compute uniform 后编码，会受到 `Queue.WriteBuffer` 的提交时序约束。

## 仍需单独设计的部分

1. **ShadowCaster 语义。** 当前阴影生成仍去掉 Fragment，不能保留 alpha clip/discard。
   无 tag 过滤时，同一 SubMesh 的多个材质 Pass 也可能重复投影。应引入明确的 ShadowCaster 选择规则与片元变体。
2. **级联质量。** `cascade_fade` 数据已经上传，但内置采样函数没有实现级联混合；当前 texel snapping
   在跟随相机的 light-view 中计算，稳定性有限。需要覆盖相机平移、边界混合和阴影距离的图像回归。
3. **变形物体裁剪。** 蒙皮使用静态网格包围球，实例混合正负缩放无法由一次全局 winding 翻转正确处理，
   shear 下的最大轴长度也不一定覆盖包围球。应提供可覆写 bounds，并为动画/实例建立保守 bounds 策略。
4. **透明排序。** 本轮保证队列优先级并按当前相机深度排序；预设中的“全部背面，再全部正面”仍不能保证
   相交透明对象的正确混合。需要按对象交错双面绘制，或可选 OIT 路径。
5. **资源类型与反射。** WGSL 反射仍是受限解析器，无法完整表达入口点可达绑定、整数纹理、不可过滤 Float32、
   depth array/cube、自定义对齐等信息。Compute 使用自动布局，未使用的声明也可能被反射进绑定。
   应允许显式布局并统一资源描述；完整 GPU 验证还需要 Error Scope/回调，不能只靠非空句柄。
6. **完整 SRP 扩展点。** 尚没有脚本可执行的 Pass 回调/RendererFeature、编译缓存、资源导入、历史资源和暂态分配器。
   `Camera3D` 的 priority/target/clear 属性也没有成为自动多相机调度规则。应先明确这些 API 的执行契约。

保留全部候选绘制项让多相机结果正确，但增加了 CPU 收集和 uniform 上传量。场景规模扩大时，
可在图编译后按使用的相机/阴影视锥做候选联合裁剪，并缓存各视图的裁剪结果。

## 验证

原生 Windows 构建：`cmake --build --preset win64-vendored-ruby --target Game --parallel 8`。

依赖排序的独立测试无需 GPU，在 VS Developer PowerShell 中执行：

```powershell
cl /nologo /std:c++20 /EHsc /I. test/scene3d/render_graph_order_test.cc /Fe:build/scene3d-order-test.exe /Fo:build/scene3d-order-test.obj
build/scene3d-order-test.exe
```

GPU 回归使用已编译的 Game 和本机 Ruby，仅用 Ruby 打包 RGSS Marshal 脚本：

```powershell
python test/scene3d/run_regression.py --game build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe
```

结果位于 `build/scene3d-regression/scene3d-results.txt`。测试覆盖多 Pass、前置消费者、MRT 覆写与重建、
MSAA、相机覆写、阴影重置、Compute 状态、顶点材质绑定、悬空 owner，以及错误图恢复。
日志中的五个 World3D 错误是专门的负向用例，其他错误使测试失败。

本轮实际验证平台为 Windows / Vulkan / NVIDIA RTX 3050 Laptop；其他后端尚未运行回归。
