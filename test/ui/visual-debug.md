# Windows 识图调试记录

2026-11-11，Windows 150% 显示缩放，NVIDIA RTX 3050 Laptop GPU。
使用当前源码构建 Game.exe，直接操作 `test/ui/demo/Game.exe`。

发现演示默认 nearest 显示采样使中文细笔画不均，窗口截图出现笔画缺失。
D3D12 和 OpenGL 均可复现；`Graphics.snap_to_bitmap` 的原始图像中文字完整。
`run.ps1` 已显式使用 `PresentSampler=linear`，实机复查后文字可读。

实机验证通过：

- 科技蓝、羊皮纸、像素绿换肤；单独金色皮肤保持不变。
- Toggle 回调；Slider 拖动与 ProgressBar 同步。
- ScrollPanel 滚轮、遮罩、ScrollBar 拖动到底和最后条目的点击命中。
- Dropdown 最后一项选择；List 鼠标选择、方向键导航、Enter 确认。
- 中英文混合输入、Enter 提交、Ctrl+A 选区、多行换行。
- 换肤保留文本、选中项、滚动位置与音量。
- 模态阻挡背景点击、Escape 关闭和焦点恢复。
- 旋转按钮命中、禁用按钮不响应、九宫格大小按钮。

自动验证：完整 Windows Game 目标构建成功；`run.ps1` 的 24 项引擎回归全部
通过；`ctest --test-dir build/ui-math-vs -C Release --output-on-failure` 通过。
触摸、手柄、真实 IME 候选窗和其他平台未在本次桌面操作中验证。
