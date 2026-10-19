# 第三方依赖的本地补丁

`third_party/` 下的依赖以子模块形式引入，正常情况下**不修改**。下面每一项都是上游缺陷的
临时修补：升级子模块时要么把同等改动带过去，要么先确认上游已经修好。

## `SDL` —— 蓝牙 Xbox 手柄在 Windows 上不可见

- 文件：`src/joystick/hidapi/SDL_hidapi_xboxone.c`，`HIDAPI_DriverXboxOne_IsSupportedDevice()`
- 引入问题的上游提交：`d5fca9628` *"Don't use the fake HID endpoint for Xbox controllers on Windows"*
- 现象：手柄在系统里正常（设备管理器里是 `HID-compliant game controller`，VID `045E` /
  PID `0B13`），但引擎完全看不到它，`SDL_GetJoysticks()` 返回 0，HIDAPI 的调试日志里是
  `driver = NONE (DISABLED)`。
- 原因：那个检查为了跳过 **XGIP（USB）** 手柄的"假 HID 端点"，把所有以 `\\?\HID#` 开头的
  设备路径都拒掉了。**蓝牙** Xbox 手柄的真实端点路径同样以 `\\?\HID#` 开头
  （`\\?\HID#{00001812-...}_Dev_VID&02045e_PID&0b13...`），于是没有任何驱动认领它：
  HIDAPI 的 Xbox 驱动拒绝它，GIP 驱动又明确声明"蓝牙手柄走 HID 而不是 GIP"。
- 补丁：把判定限定在非蓝牙设备上。

  ```c
  -    if (device && SDL_strncmp(device->path, "\\\\?\\HID#", 8) == 0) {
  +    if (device && !device->is_bluetooth && SDL_strncmp(device->path, "\\\\?\\HID#", 8) == 0) {
  ```

- 验证：打补丁后同一行日志变成 `driver = SDL_JOYSTICK_HIDAPI_XBOX_ONE (ENABLED)`，
  引擎输出 `[Gamepad] 1 joystick(s), 1 gamepad(s)` 与
  `[Gamepad] connected: Xbox Series X Controller`。
