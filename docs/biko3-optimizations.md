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

2026-09-30 复查当前设备目录后，适配包已包含 `ddraw.dll`，但缺少同目录的配套 `d3d9.dll`。后者从设备读回并核对 SHA-256 `43555a32bdf6e3509461c4761012cb38bd8b636025ca879115f685cea9738eb6` 后纳入 Biko3 v1 的整文件替换清单；`Data/catalog` 仍由启动配置创建。此时只完成打包和主机验证，双 DLL 安装后的真机启动尚待验证。

同日对比成功真机日志与另一份闪退日志：成功启动从游戏目录加载了 `ddraw_.dll` 并创建 `IDirect3DTnLHalDevice`；闪退启动找不到 `ddraw_.dll`，回退系统 `ddraw.dll`，在字体创建失败后创建 `IDirect3DRGBDevice` 并访问违例。Sarek 的 `GetProxiedDDrawModule()` 优先加载 `ddraw_.dll`，失败时会回退到系统 `ddraw.dll`。从成功设备读回的 Wine DirectDraw 代理（PE32/i386，700416 字节，SHA-256 `7fed4325a623d9a1c83a05f145557ee2899a4ca035e0025f06ab7afe7fdf044b`）已按要求作为第三个游戏目录文件纳入包。

随后在同一台 Switch、同一个 `Biko3` 目录做了对照：临时停用 `ddraw_.dll`，保持已核对的 `ddraw.dll`、`d3d9.dll` 与 DXVK 图形后端，`Biko_DVD.exe` 仍回退系统 `ddraw.dll`、创建 `IDirect3DTnLHalDevice` 并至少提交 90 帧。再用 SHA-256 相同的 EXE 副本命名为 `biko3DVD.exe`，保持同一目录和 DLL，日志记录到第 560 帧，也未出现字体错误。故 `ddraw_.dll` 与原 EXE 文件名都不是启动的必要条件；旧日志中的字体初始化失败尚未复现，不能归因于这两个差异。游戏 `Type_G.FTT`、`Type_S.FTT` 与成功设备上的文件哈希一致，顶层 167 个 `Data` 文件名称和大小一致。新增三 DLL 适配包的安装与持续运行仍未单独验收。

同日重装完整主程序后，在原来成功的 Switch 上复现了相同的 `0043F1C8` 空指针读取。详细日志确认 FreeType 2.13.3 与 Switch 共享字体正常初始化，但 `HKLM\Software\illusion\Bikou3_DVD` 不存在。游戏读取 `INSTALLDIR` 失败后退回 `GetCurrentDirectoryA()`，在主启动分支直接拼接 `Data`，形成错误的 `C:\Biko3Data\`；配置、字体缓存与游戏资源均从这个不存在的目录打开。`Type_S.FTT` 创建返回 `c000003a`（路径不存在），随后游戏报告字体创建失败并继续初始化，最终访问空对象。反汇编显示这个字体错误分支检查的是字体缓存生成/写入函数的返回值，不能等同于 `CreateFontA` 失败。重装前的详细日志在同一个注册表读取位置成功打开该键并查询 `INSTALLDIR`。游戏 EXE、三份 DLL、两份 FTT 及 `setting.cfg` 在重装前后 SHA-256 一致，因此已定位到缺失的游戏安装目录注册信息；设备只补回 `INSTALLDIR=C:\Biko3\` 后重新启动，日志恢复创建 `IDirect3DTnLHalDevice` 并记录 640×480 画面提交成功，未再出现字体错误、D3D9 纹理创建失败或 `0043F1C8` 崩溃。当前日志记录到前四次画面提交，因此只确认启动恢复，持续运行仍需另外验收。该值需要末尾反斜杠，并应按实际游戏目录生成；仅增加 `ddraw_.dll` 或 `Data/catalog` 不能修复此路径。

随后新增通用单游戏设置 `registry-install-dir=Software/illusion/Bikou3_DVD`（API 18），纳入 Biko3 v1 测试适配包。每次启动根据所选 EXE 的实际 DOS 目录生成末尾带反斜杠的 `INSTALLDIR`，缺失时创建，移动目录后更新，保留其他注册项；通过原生 NT 注册表 API 写入并在游戏加载前持久化。未配置的游戏保持原行为。定向 NRO 编译、注册表主机回归和适配包回归通过；三份 DLL 与已核对的真机读回文件哈希一致，ZIP 清单和依赖检查通过。未运行完整 CI。

2026-09-30 在用户彻底关闭 AutoRun 后，备份并删除真机的整个 `Software\\illusion\\Bikou3_DVD` 子键，上传新 NRO（SHA-256 `1440cf5a6fc8fdf4c19ba349f09f752d0d287c0d2972f55286c54143bea13489`）及启用注册表补齐的游戏配置，逐项读回校验。随后清除了在线适配包管理表缓存；用户测试后确认该问题已修复。新一轮日志在游戏加载前出现 `[GAME REGISTRY] HKLM/Software/illusion/Bikou3_DVD INSTALLDIR from C:\\Biko3\\Biko_DVD.exe: updated status=00000000`，真机注册表重新生成 `INSTALLDIR=C:\\Biko3\\`，并创建 `IDirect3DTnLHalDevice`、成功提交 640×480 画面。由此确认缺失注册项的自动补齐与本次启动闪退修复；新增三 DLL 包的启动器安装流程、持续运行及其他游戏功能仍未逐项验收。

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
