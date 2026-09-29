# Biko3 Switch 保留版

已验证的游戏程序为 `Biko_DVD.exe`（SHA-256 `7c02023d9acda74b16431eea2d0c4e610df35e6fa1802bcbfe8a78994923a495`）；补丁安装不依赖文件名，仍只适配该程序的已核对代码布局。游戏 EXE 不落盘修改。D7VK Sarek 源码已直接纳入 `third_party/dxvk-sarek-biko3/`，基线为 [`v1.13.0`](https://github.com/pythonlover02/dxvk-sarek/tree/v1.13.0)，提交 `37f397e142b977a343e920dbc4c7bf7ed2c63a81`。上游版权及许可证保留在该目录中。

`third_party/dxvk-sarek-biko3/src/ddraw/d3d7/d3d7_device.cpp` 直接包含三处当前 DDraw 行为；`src/util/log/log.h` 包含一个必要的 i386 Wine 日志调用约定修复：

| 保留项 | 结论 |
| --- | --- |
| 不透明队列排序缓存 | 同场景的一轮设备测量约 16.1 → 17.85 FPS；用户体感不明显。缓存计划改变时或每 128 次命中会执行原排序并校验，差异会停用缓存。透明排序照常执行。 |
| 保守墙面远距跳过 | 同视角顺序对照约 17.90 → 18.10 FPS，提升很小。先验证 128 次候选，之后每 128 次复核；返回值或已跟踪状态有差异即停用。贴墙、画面测试正常。 |
| 无用三角形可见性块跳过 | 两个已核对调用者都丢弃该块的返回值；设备画面、碰撞正常，未测得明确帧率收益。按用户要求保留。 |
| `__wine_dbg_output` i386 调用约定 | Sarek 基线声明与 Wine 的 `__cdecl` 导出不符；修复曾消除启动栈损坏，属于运行前提。 |

补丁由单游戏配置 `d7vk-biko3-patches=1` 启用，不再检查 EXE 名称；D7VK 仍在安装前校验映像基址、映像大小和原指令序列，不匹配即跳过。默认值为 `0`，故其他游戏不会自动安装这些补丁。配置文件名仍按启动的 EXE 命名；如重命名程序，需同步重命名其 `.wine-nx.txt` 配置。游戏 EXE 不落盘修改。一次性采样、QPC 计时、统计日志及失败实验均已移除。颜色和离屏窗口居中修复在本仓库提交 `1fab115`。Vulkan RGBA 视图修正也使用单游戏开关：Switch 默认 `vulkan-fs-hack-rgba-view=0`，Biko3 设备配置设为 `1`。Biko3 配置还保持 `verbose=0`、`locale=ja_JP.UTF-8`、`wined3d-csmt=0`、`window-fit=1`、`d7vk-offscreen-opengl=1`。

2026-09-29 已将新 NRO、Biko3 目录的 `ddraw.dll` 和启用此开关的配置上传并读回核对；旧文件分别在设备同目录以 `.before-20260929-switchgate` 后缀备份。编译和主机适配包回归已通过。新一轮真机日志含开关启用记录和四处预期的补丁写入，用户确认启动、画面及操作正常。尚未实测重命名 EXE。

鉴赏入口需要可写的 `C:\Biko3\Data\catalog` 目录。2026-09-29 真机日志显示游戏尝试生成 `00000.bmp` 至 `00024.bmp` 时全部收到路径不存在错误，随后读取 `00000.bmp` 失败并在未检查贴图创建结果的情况下崩溃。补建空目录后游戏生成了全部 25 个 BMP，用户确认鉴赏可以进入；这次未改 EXE、运行时或图形 DLL。

后续把目录补齐做成通用单游戏设置 `ensure-game-dir=Data/catalog`，并纳入 Biko3 v1 适配包（API 15）。运行时每次启动前检查并补齐目录；目录已存在时保持其中的图片不变。适配包不包含 Biko3 的游戏目录 `DDraw.dll`，仍需使用单独部署的已验证版本。新 NRO 与启用此设置的 Biko3 配置已上传并读回校验；本轮真机日志出现 `[GAME DIR] ensured sdmc:/switch/wine/drive_c/Biko3/Data/catalog`，用户重启后确认鉴赏正常进入。适配包的启动器安装流程尚未实机测试。

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

**验收边界：**精简版完成编译和设备字节读回；之前三项优化的合并诊断版曾通过画面和碰撞测试。本仓库还删除了离屏 pbuffer 创建成功的额外日志；定向 NRO 已部署，鉴赏入口已由用户实机验证。低帧问题仍未解决，约 17–19 FPS 的同场景结果不能外推到其他场景；视频播放、音频、存档和长时运行未在本轮验收。当前源码不包含后续无收益的视线同侧跳过、D3D9 状态去重或视图逆矩阵缓存。
