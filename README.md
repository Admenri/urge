# ![Logo](app/resources/urge_favicon_64.png) 通用Ruby游戏引擎 URGE

## 语言选择
- [English](README_EN.md)
- [简体中文](README.md)

## 概览

 - URGE 是一款兼容RGSS 1/2/3系API的游戏引擎。
 - 使用了 SDL3 作为底层库，支持运行于 Windows, Linux, Android, WASM/Emscripten 等平台。
 - 使用了 DiligentCore 作为渲染RHI，提供了 D3D12/Vulkan 等现代API的支持。  
 - 本项目使用 BSD-2-Clause 许可证开源。  
 - 项目风格来自 The Chromium Project。  
 - 项目启发于 Chromium，RGModern。  
 
## 截图

<img src="app/test/1.png" height="400">
<img src="app/test/2.png" height="400">
<img src="app/test/3.png" height="400">
<img src="app/test/4.png" height="400">
<img src="app/test/5.png" height="400">
<img src="app/test/6.png" height="400">

## 特性

- 多线程: 引擎的游戏逻辑与渲染逻辑是分开于独立两个线程的，可以最大限度利用多核处理器的优势。  
- 现代图形API: 得益于 DiligentCore 的渲染能力，使得引擎支持了 D3D12/Vulkan 这类现代图形API，相比 OpenGL 家族有了更好的性能。  
- 跨平台: 引擎支持 Windows, Linux, Android, WASM/Emscripten 等平台。  
- 高性能: 游戏脚本层使用了解耦方式处理，可接入 CRuby，MRuby，Crystal 等语言后端来最大限度提升脚本语言处理速度。  

## 构建方式

1. 确保 CMake版本 ≥ 3.20。  
2. 执行克隆
```
git clone --recursive https://github.com/Admenri/urge.git
```
3. 执行构建脚本
```
cmake -S . -B out
```
4. 执行编译
```
cmake --build out --target urge_engine
```

## 第三方库

### 包含于项目源码中
- SDL_image - https://github.com/libsdl-org/SDL_image  
- SDL_ttf - https://github.com/libsdl-org/SDL_ttf  
- dav1d - https://github.com/videolan/dav1d  
- imgui - https://github.com/ocornut/imgui  
- concurrentqueue - https://github.com/cameron314/concurrentqueue  
- inih - https://github.com/benhoyt/inih  
- rapidxml - https://rapidxml.sourceforge.net/  
- magic_enum - https://github.com/Neargye/magic_enum  
- miniaudio - https://github.com/mackron/miniaudio  
- sav1 - https://github.com/SAV1-org/SAV1  
- libffi - https://github.com/libffi/libffi  
- spine-runtimes - https://github.com/EsotericSoftware/spine-runtimes  

### 外部引用
- SDL3 - https://github.com/libsdl-org/SDL  
- DiligentCore - https://github.com/DiligentGraphics/DiligentCore  
- freetype - https://github.com/freetype/freetype  
- physfs - https://github.com/icculus/physfs  
- zlib - https://github.com/madler/zlib  
- ogg - https://github.com/xiph/ogg  
- vorbis - https://github.com/xiph/vorbis  
- ruby - https://github.com/ruby/ruby  
- spdlog - https://github.com/gabime/spdlog  
- opus - https://github.com/xiph/opus  
- opusfile - https://github.com/xiph/opusfile  
- mimalloc - https://github.com/microsoft/mimalloc  

## 联系方式

- 邮箱: admin@admenri.com  
- 网站: https://admenri.com/  

© 2015-2025 Admenri
