# Biko3 Switch 保留版

目标仅为 `Biko_DVD.exe`（SHA-256 `7c02023d9acda74b16431eea2d0c4e610df35e6fa1802bcbfe8a78994923a495`）。游戏 EXE 不落盘修改。D7VK Sarek 源码已直接纳入 `third_party/dxvk-sarek-biko3/`，基线为 [`v1.13.0`](https://github.com/pythonlover02/dxvk-sarek/tree/v1.13.0)，提交 `37f397e142b977a343e920dbc4c7bf7ed2c63a81`。上游版权及许可证保留在该目录中。

`third_party/dxvk-sarek-biko3/src/ddraw/d3d7/d3d7_device.cpp` 直接包含三处当前 DDraw 行为；`src/util/log/log.h` 包含一个必要的 i386 Wine 日志调用约定修复：

| 保留项 | 结论 |
| --- | --- |
| 不透明队列排序缓存 | 同场景的一轮设备测量约 16.1 → 17.85 FPS；用户体感不明显。缓存计划改变时或每 128 次命中会执行原排序并校验，差异会停用缓存。透明排序照常执行。 |
| 保守墙面远距跳过 | 同视角顺序对照约 17.90 → 18.10 FPS，提升很小。先验证 128 次候选，之后每 128 次复核；返回值或已跟踪状态有差异即停用。贴墙、画面测试正常。 |
| 无用三角形可见性块跳过 | 两个已核对调用者都丢弃该块的返回值；设备画面、碰撞正常，未测得明确帧率收益。按用户要求保留。 |
| `__wine_dbg_output` i386 调用约定 | Sarek 基线声明与 Wine 的 `__cdecl` 导出不符；修复曾消除启动栈损坏，属于运行前提。 |

补丁仅在 EXE 名称、映像基址和原指令序列匹配时安装。运行时核验是错误回退保护；一次性采样、QPC 计时、统计日志及失败实验均已移除。颜色和离屏窗口居中修复在本仓库提交 `1fab115`，设备配置保持 `verbose=0`、`locale=ja_JP.UTF-8`、`wined3d-csmt=0`、`window-fit=1`、`d7vk-offscreen-opengl=1`。

重建时将 llvm-mingw 的 `bin` 加入 `PATH`，在仓库根目录运行：

```sh
meson setup /path/to/build third_party/dxvk-sarek-biko3 \
  --cross-file third_party/dxvk-sarek-biko3/cross-i686-wine-nx.txt \
  -Dbuildtype=release -Dstrip=true -Dwrap_mode=nodownload \
  -Denable_ddraw=true -Denable_d3d9=true \
  -Denable_dxgi=false -Denable_d3d8=false \
  -Denable_d3d10=false -Denable_d3d11=false
ninja -C /path/to/build src/ddraw/ddraw.dll
```

本次从仓库内源码重新定向构建通过，产物为 PE32/i386。它与此前部署的精简版 DLL 等长，仅 PE 时间戳 4 字节不同；此前产物及设备 `/switch/wine/drive_c/Biko3/ddraw.dll` 的 OpenMTP 读回 SHA-256 均为 `15002e10bb2821bc3cf087e8dac5679a8dcde62dac9a54eacb84d6645b9f2231`。原版 `d3d9.dll` 为 `43555a32bdf6e3509461c4761012cb38bd8b636025ca879115f685cea9738eb6`。

**验收边界：**精简版只完成编译和设备字节读回，尚未重新启动游戏。之前三项优化的合并诊断版曾通过画面和碰撞测试。本仓库还删除了离屏 pbuffer 创建成功的额外日志；定向 NRO 构建通过，但新 NRO 没有部署。低帧问题仍未解决，约 17–19 FPS 的同场景结果不能外推到其他场景；视频、音频、存档和长时运行也未在本轮验收。当前源码不包含后续无收益的视线同侧跳过、D3D9 状态去重或视图逆矩阵缓存。
