# Part I: WMA / MIDI 音频解码

本目录为 URGE 内置的 C++ 解码器。音频仍经过 miniaudio 混音和 SDL 输出，沿用现有音量、变速、暂停、定位、淡出和 BGM/BGS/ME/SE 通道；不新增设备后端，不依赖 FFmpeg、Rust、FluidSynth、Homebrew 或操作系统 MIDI 合成器。

## 配置与音源

```ini
[Audio]
; 可选；留空自动查找系统音源。相对路径由游戏文件系统读取。
MIDISoundFont = Fonts/MyFont.sf2
```

音源按以下顺序尝试，成功后由当前音频服务共享，不反复读取：

1. `MIDISoundFont` 指定的 `.dls` 或 `.sf2` 文件，支持游戏内路径及绝对路径，按文件内容判断格式。
2. Windows 的 `%SystemRoot%/System32/drivers/gm.dls`，或 macOS 的 `/System/Library/Components/CoreAudio.component/Contents/Resources/gs_instruments.dls`。
3. 若均不可用，记录原因并关闭 MIDI；WMA 和其他音频不受影响。

Linux、Android、Web 不假定有系统音源，需要随游戏提供可用文件。音源在首次播放 MIDI 或调用 `Audio.setup_midi` 时加载。配置在重新启动游戏后生效。系统音源仅在本机读取，不随 URGE 分发。原先未接入播放器的 `Soundfont` 配置由 `MIDISoundFont` 取代。

## MIDI 与循环

支持 SMF 格式 0/1 和 RIFF RMID 包装，PPQN/变速事件及 SMPTE 时间分割。合并各轨事件，按采样帧调度；支持程序/Bank 选择、音符与力度、音量、表情、声像、延音踏板、弯音及 RPN 弯音范围、All Notes Off、All Sound Off、Reset All Controllers。第 10 通道使用打击乐音色，最多 128 个采样声部，超出时淘汰最早声部。

**RGSS Controller 111**：按时间顺序出现的第一个 CC111 记录为循环起点，不限制通道或控制器值。首次播放从曲首开始；开启循环时，在曲尾最大 End Of Track 时间跳回该点，没有有效标记则回到曲首。跳回会重建标记前的音色、控制器和仍有效的音符状态，标记同一时刻的事件正常执行，防止挂音及音色重置。不同轨的节奏变化会计入循环时间。只有开启循环的通道使用此区间；非循环 MIDI 在曲尾释放音符，并最多保留两秒释放尾音。显式设置 `AudioStream` 的循环区间会覆盖文件的默认循环点。

- DLS：以 DLS Level 1 为最小范围，RIFF instrument/region/wave pool、pool table、键盘/力度分区、PCM 8/16/24/32 位采样、采样率、基准音高、微调、单个 forward 采样循环及常量 EG1 ADSR、声像、增益。双声道 DLS 采样混为单声道后按声部声像输出。
- SF2：RIFF hydra 的 preset/instrument/sample 及全局、局部分区，16 位 PCM、键盘/力度范围、采样/循环地址偏移、音高和音阶、衰减、声像、音量 ADSR、exclusive class。

这是基本 GM 采样合成器，不实现 DirectMusic 段落、SMF 格式 2、SF3 压缩采样、完整 DLS2/SF2 调制矩阵、滤波器、LFO、合唱、混响或厂商 GS/XG 扩展。因此音色效果不保证与原版 Windows 合成器逐样本一致。兼容旧导出器时，按状态规定的消息长度读取后，对通道参数屏蔽多余的最高位；不猜测缺失的 running status。

## WMA

支持 ASF 固定大小数据包中的单/双声道 WMA v1/v2（格式标签 `0x0160` / `0x0161`），最高 48 kHz，包括媒体对象重组、compressed payload、位储存器、变长块、指数 VLC/LSP 包络、系数 VLC、噪声替代、联合立体声、逆变换和重叠相加。初始变换延迟被移除，输出为交错 float PCM。定位通过重置解码器并重建前缀状态完成；长音频定位的成本随目标位置增加。

不支持 WMA Pro、Lossless、Voice、DRM 或 ASF 可变大小数据包，遇到这些格式会记录不支持原因。音频中损坏的帧以对应时长的静音替代，在后续包边界恢复，并记录一次警告。

## 文件边界与维护

所有 RIFF/ASF 子块都以父块长度校验，文件偏移、音源表索引、采样/循环范围和音符参数不会作为宿主指针。音源/ASF 文件上限 256 MiB，MIDI 上限 64 MiB，音源采样上限 64M 个 float、分区上限 65,536，SF2 分区遍历另有总预算。MIDI 事件上限一百万、时长上限 24 小时。不可用或不支持的资源返回音频打开失败，沿用调用方既有的错误处理；解码器不直接退出进程。

- `reader.h`：有边界的 RIFF 与字节读取。
- `soundfont.cc`：DLS/SF2 到共享采样分区。
- `midi.cc`：事件调度、采样合成、状态重建及 CC111。
- `asf.cc`、`wma.cc`、`wma_tables.inc`：ASF 拆包、WMA 解码及只读码表。
- `backend.cc`：miniaudio data source 适配及默认循环区间。

参考 OxideAV、libdmusic、TinySoundFont/TinyMidiLoader 的 MIT 实现，版本和许可见 [LICENSES.md](LICENSES.md)。CC111 的资源约定见 [RPG Maker VX 资源规范](https://www.rpg-maker.fr/dl/monos/aide/vx/source/rpgvx/resource.html)。


# Part II: Third-party reference notices

The C++ readers and synthesizer are maintained in URGE. They use no Rust,
FFmpeg, FluidSynth, libdmusic, or TinySoundFont runtime library.

The following MIT implementations informed the format parsing and synthesis.
WMA wire tables in `wma_tables.inc` are adapted from OxideAV.

## OxideAV WMA

Reference: [4ef59994ea5a19c9ddb87649c066f7e85f627c15](https://github.com/OxideAV/oxideav-wma/tree/4ef59994ea5a19c9ddb87649c066f7e85f627c15).

```text
MIT License

Copyright (c) 2026 Karpelès Lab Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## libdmusic

Reference: [790662e71120ff70359033083eb5a8a5751d5300](https://github.com/libdmusic/libdmusic/tree/790662e71120ff70359033083eb5a8a5751d5300).

```text
MIT License

Copyright (c) 2019 Francesco Bertolaccini

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## TinySoundFont / SFZero

Reference: [853a0a171759f1ddba0de1442133a75912bbeffa](https://github.com/schellingb/TinySoundFont/tree/853a0a171759f1ddba0de1442133a75912bbeffa).

```text
Copyright (C) 2017-2025 Bernhard Schelling (Based on SFZero, Copyright (C) 2012 Steve Folta, https://github.com/stevefolta/SFZero)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

