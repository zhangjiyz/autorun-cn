# AutoRunNX 当前工程状态

更新时间：2026-09-28。本次新增 Biko3 交接记录；下方其他游戏及发布基线保留原日期的历史语境，不代表已经重新核对。此文件不把尚未完成的真机实验写成已支持。

## Biko3 / 尾行3：2026-09-28 暂停点

用户要求先记录进度并自行提交，明天再继续适配。当前实际分支是 `merge/upstream-20260926`，记录时 HEAD 为 `131f24af768b122eb0b998c3dc57a6cef64dbfb6`；不要根据下方历史基线擅自切回 `main_cn`。本轮改动尚未提交或推送，也未暂存。

### 实机结论与待验收项

- 用户已确认现在能进入游戏，原先黑屏/启动闪退已越过；这不等于完整兼容。
- 用户反馈游戏内约 10 FPS。改动前日志的复杂场景约 7–8 FPS，部分菜单/简单场景约 60 FPS。
- 用户反馈视频一播放就闪退，随后明确要求“先不管闪退”。视频问题保持搁置，尚未确认新的崩溃位置，也没有补齐视频解码器。
- 当前最新的缓冲区更新合并版本已经部署，但还没有用户复测反馈；不能声称它提高了真机帧率。
- 音频初始化成功、输入日志有按键，不代表音乐/音效、完整键位、存档/读档、重启或持续运行已经验收。

### 目标与设备配置

- 本地游戏：`/Users/luoqi/Downloads/autorun测试/Biko3/`；设备目录：`/switch/wine/drive_c/Biko3/`。
- 设备目标：`sdmc:/switch/wine/drive_c/Biko3/Biko_DVD.exe`，不是原来的 `biko3.EXE`。本地对应 `Biko３_DVD.exe`，1847340 字节，SHA-256 `7c02023d9acda74b16431eea2d0c4e610df35e6fa1802bcbfe8a78994923a495`；PE32/i386，image base `0x400000`、entry RVA `0x135e44`。原 EXE 的自修改代码路径曾失败，不要混用日志。
- DVD EXE 使用 DirectDraw 7 / Direct3D 7、DirectSound/Input，开场媒体走 AMStream/DirectShow。
- 当前 `/switch/wine/drive_c/Biko3/Biko_DVD.wine-nx.txt` 已读回核对，保持如下 44 字节内容：

```ini
verbose=0
locale=ja_JP.UTF-8
wined3d-csmt=0
```

- 实际后端是 WineD3D/OpenGL。显式缓冲刷新使用默认值；线程采样关闭。原 `biko3.box64.txt` 诊断配置已改名为 `.disabled-interpreter-20260928-002008`，不要恢复它来测试 DVD EXE。
- 原安装日志证实需要 `HKLM\Software\illusion\Bikou3_DVD` 下 `INSTALLDIR="C:\Biko3\"`（末尾反斜杠）。设备原先缺此项，已向新读取的 `system.reg` 追加，并用实际解析器及读回验证；原文件前缀保留。备份是 `/switch/wine/registry/system.reg.before-biko3-20260928-004224`。
- 没有修改游戏 EXE、视频、字体缓存或存档。字体初始化的长扫描与大量 glyph 查询不应直接视作死锁，也不要盲目覆盖设备已有的字体缓存。

### 当前源码改动

| 文件 | 修改与边界 |
|---|---|
| `dlls/win32u/winnx_opengl.c` | 仅 `WineD3D_OpenGL` 能力探测窗口使用独立 1×1 EGL pbuffer，避免 format 27 的探测面与 format 22 的实体屏幕冲突。增加 surface/context/EGL make-current 诊断；离屏面不做实体屏幕交换。实际游戏窗口仍走原屏幕路径。 |
| `dlls/win32u/opengl.c` | Switch 下直接记录 HDC 格式缺失/不匹配，使 quiet 日志也能定位 WGL 激活失败。 |
| `dlls/ntdll/unix/horizon.c` | 批量销毁窗口时删除当前节点后留在当前链表链接，仅跳过不匹配窗口时向后移动，修复跳过节点及空指针解引用。 |
| `dlls/quartz/filtermapper.c` | `CreateClassEnumerator` 返回 `S_FALSE` 时不再解引用空类别枚举器；释放工厂并创建可用的空结果枚举器，空数组不分配/复制零项。修复了实际崩溃 PC `quartz.dll+0x1a851` 的路径；没有实现 MPEG 解码。 |
| `dlls/wined3d/buffer.c` | 已整段标记的缓冲不再追加小范围；只合并与最后一段连续或重叠的更新，保存准确并集，保留未更新的间隙。不延迟提交，也不跳过 GPU 必需的刷新。这是共享 WineD3D 行为优化，尚待 Biko3 真机效果验证。 |
| `dlls/ntdll/unix/virtual.c` | 保留 `gl_top` 耗时榜，增加每 10 秒的 `gl_hot=接口编号:次数` 前四项；以原子读取汇总计数。无需开启采样即可读取调用密度。 |

新增的四个测试也要纳入用户提交：`wine-nx-probe/tests/check_window_destroy.py`、`check_filtermapper_empty.py`、`check_buffer_dirty_ranges.py`、`check_gl_profile_counts.py`。

### 失败实验与性能证据

- pbuffer 探测修复之后，GL 4.4 创建仍会失败，但可以回退并建立 3.2 上下文。不要单凭 4.4 的 `EGL_BAD_MATCH` 判断最终 GL 初始化失败。
- CSMT 开启时，工作线程 `000c` 的上下文激活失败，backup DC 也返回 `0x7d0`，继而 DirectDraw swapchain 初始化失败。当前用单游戏 `wined3d-csmt=0` 绕过；根因未解决，不能直接重新开启 CSMT。
- 此前另有两次 `ddraw:d3d_device_init` 设置渲染目标失败 `0x8876086c`；后续用户已能进入游戏，不应把这一条旧错误当成当前必定黑屏，也不能当成已独立修复。
- 关闭 `wined3d-explicit-buffer-flush` 并开启 `profile=1` 的实验确实生效，但低帧场景仍约 5.45–7.0 FPS，未见改善；场景与采样开销不同，不能把所有差异归因于单个开关。该配置已回退，原 44 字节配置读回一致。
- 该实验中每帧约 1.4–1.5 万次 OpenGL Unix 调用；刷新约 2.5 万次/10 秒（每帧约 400 多次）。主线程占用单核约 82–87%。采样末轮约 x86 50.6%、native 34.4%、svc 14.7%，其中游戏 x86 18.8%、WineD3D x86 12.5%；采样包含阻塞时间，不是纯 CPU 工作比例。
- 实际设备 WineD3D 的 `.text` 与本地改动前的 PE 一致。`wined3d.dll+0x237e7` 是 `flush_bo_ranges` 的 `glFlushMappedBufferRange` 循环；`+0x3a757` 是 `wined3d_cs_exec_draw`，`+0x26dee/+0x26eff` 是 `context_apply_draw_state`。
- 动态缓冲在 `buffer.c` 强制 `coherent=false`，因此关闭显式刷新没有消除主要刷新调用；改变 coherent 分配后仍有 GPU 地址映射开销。不要把无效的配置实验重新当作已确认的性能方案。

### 当前部署与回退

最新批次是外层工作目录的 `diagnostics/biko3-buffer-merge-20260928-015205/`。其中有源码 diff、测试记录、原 WineD3D、当前 NRO/ELF、部署清单及 `rollback.mtp`。部署采用 staged 上传、检查远端尺寸、改名备份再激活；配置保持不变且读回一致。NRO/DLL 未做部署后完整字节读回，下面是本地产物的 SHA-256。

| 当前设备文件 | 大小与本地产物 SHA-256 | 设备备份 |
|---|---|---|
| `/switch/wine/wine-nx-runtime.nro` | 38895664；`ae95bdc30f1fb7093bc96157ee1532b775b5c9c0a567f49ae3c3977a6699110e` | 同路径后缀 `.before-buffer-merge-20260928-015205` |
| `/switch/wine/drive_c/windows/syswow64/wined3d.dll` | 4104192；`e34221b1a004b4b133a7ee10a4e6c4ff2d855df40d4239d8bcd33d0b088696e0` | 同路径后缀 `.before-buffer-merge-20260928-015205` |
| `/switch/wine/drive_c/windows/syswow64/quartz.dll` | 856064；`126e1f46a089f20f18f610cbd471b0dd4810779c8559ef2698256d7578dafb94` | 同路径后缀 `.before-biko3-empty-20260928-012955` |

需要回退最新性能实验时，只恢复该批次 runtime/WineD3D；Quartz 空枚举修复、注册表安装路径和 csmt=0 是已经让游戏进入游戏的组合，不要一起恢复到最初黑屏状态。回退前先检查实际远端文件是否已被后续构建替换。

### 已做验证与复现命令

- 四个新增测试均通过 macOS UBSan 与 Docker Linux ASan/UBSan。macOS Xcode ASan 曾停在 dyld/分配器初始化，因此脚本在 Darwin 使用 UBSan，不是游戏崩溃证据。
- 脏区测试验证相邻/倒序/重叠/包含/间隙、整段更新、OOM、UINT_MAX 边界和 50000 次随机更新的准确字节覆盖。合成测试中连续 10000 次 4 字节更新从 10000 段减为 1 段，不能把这个数字当作真机帧率收益。
- GL 汇总测试验证次数/耗时独立排序、零耗时调用、区间增量、短缓冲与零长度输出。Quartz、脏区、GL 汇总的修复前源码被对应回归拒绝。
- Linux 的 `check_wow64_unix_tables.py` 验证了 Unix 分发及表边界。定向 Switch NRO、i386 Quartz 与 WineD3D 编译通过；`git diff --check` 通过。未运行完整 CI。

```sh
# cwd: autorun-cn；仅重建相关目标
/usr/local/bin/docker run --rm -v "$PWD:/work" -w /work/wine-nx-probe/build-local-ci/runtime autorun-local-ci-runtime:cefedb7359e8f890 ninja -j4 wine-nx-runtime-nro
/usr/local/bin/docker run --rm -v "$PWD:/work" -w /work/wine-nx-probe/build-local-ci/pe autorun-local-ci-runtime:cefedb7359e8f890 make -j4 dlls/wined3d/i386-windows/wined3d.dll dlls/quartz/i386-windows/quartz.dll
python3 wine-nx-probe/tests/check_buffer_dirty_ranges.py
python3 wine-nx-probe/tests/check_gl_profile_counts.py
python3 wine-nx-probe/tests/check_filtermapper_empty.py
python3 wine-nx-probe/tests/check_window_destroy.py
```

### 明天继续的顺序

1. 用户完全退出并重启 `Biko_DVD.exe`，在相同低帧场景运行约一分钟，再正常退出并回到 MTP。不要未经用户要求自行启动设备游戏。
2. 下载新 `Biko_DVD.log`、`autorun_runtime.log`、当前 profile/target，核对摘要和 `[TARGET]`，以及新增 `gl_hot` 是否出现。`Biko_DVD_profiler.log` 是上一轮采样实验的文件，不能优先当作最新日志。
3. 对比同场景交换帧增量、`gl_top=863` 的刷新次数/耗时、`gl_hot` 高频接口，判断脏区合并有没有实际命中负载。没有结果前不保留“提速已修复”的结论。
4. 若仍慢，沿真正的高频接口、动态缓冲上传、逐次绘制状态设置与 x86/native 调用边界继续优化；不要同时混入 CSMT、采样、浮点参数或视频变化。
5. 若要重新启用多线程绘制，先修复已复现的 WGL 工作线程上下文激活失败，再单独复测。视频问题只有用户恢复该范围后再继续。

日志证据均在外层 `diagnostics/`，主要批次是 `biko3-csmt-result-20260928-012312`（Quartz 空指针）、`biko3-play-video-result-20260928-013840`（进入游戏的性能基线）、`biko3-perf-result-20260928-014522`（无效刷新配置实验）、`biko3-buffer-merge-20260928-015205`（最新部署）。这些日志、游戏文件、临时 DLL、NRO/ELF 和构建缓存不纳入 Git。

## Git 与发布基线

- 工作分支：`main_cn`。
- 本轮整理前 HEAD：`beec656`（`feat: extend CNB profiles and legacy game support`）。
- 整理前本地 HEAD 与 `cnb/main_cn` 一致；`origin/main_cn` 仍停在 `f9872b5`。
- CNB 远端：`https://cnb.cool/PalmMuse/autorun-cn.git`。
- 用户要求本轮创建 `AGENTS.md`、`MEMORY.md`，审计现有改动后提交并推送 `cnb/main_cn`。
- 用户不要求每次适配都跑完整 CI；完整 CI 由用户手动执行。开发侧只做风险相称的定向测试并如实记录范围。

## 本轮工作区包含的功能

### CNB 更新与本地构建

- 默认更新仓库迁移为 CNB `PalmMuse/autorun-cn`，更新器解析 CNB Release 元数据和附件 SHA-256。
- Release 主程序与适配表读取最新正式 Release；Debug 主程序与适配表可固定到测试标签并允许预发布。
- 本地构建增加 `--build-type`、`--profile-repository`、`--profile-tag` 及对应的 CMake 编译期覆盖。
- 设备上的旧 GitHub 默认地址和旧构建默认地址可迁移到当前构建地址；用户手动填写的自定义地址保留。
- 手动选择适配包允许同版本重装，方便固定测试标签替换附件；自动更新仍要求更高版本。
- NACP 版本从上游 `0.0.1` 延伸为 `0.0.1.1`。

### 启动器与适配系统

- 首页/库网格调整为 7 列 × 2 行，相关截图定位测试已同步。
- 适配包 schema 为 3，运行时 `GAME_PROFILE_API` 为 10。
- 适配包可携带封面、按键、设置、金手指定义以及 SHA-256 限定的原生二进制补丁。
- 应用二进制补丁前会备份匹配的 EXE；恢复适配包时同时恢复配置、按键和游戏程序。
- 二进制补丁不执行下载脚本；哈希或原始字节不匹配时拒绝修改。
- 配置系统新增 `aspect-fit`、`touch-coordinates` 和 `wined3d-frontbuffer-swap` 等单游戏选项。

### 音频后端

- `winenxaudio` 从单一 render client 改为最多 8 个共享 render client，并在统一的 Switch `audout` 缓冲区混音。
- 修正应用采样率与 48 kHz Switch ring 的容量单位换算，覆盖 22.05 kHz 等格式，避免 scratch buffer 越界。
- 32 位客户端可见缓冲区继续限制在可用的 32 位地址范围。
- DirectSound/Miles/Bink 同时打开音频流是本次修改的重要设备场景；主机单元测试不能代替视频、音乐和音效并发的真机验证。

### 三国赵云传

- 配置包含 `aspect-fit=800x600`、`touch-coordinates=screen`、左摇杆圆周鼠标移动和专用按键。
- 2002ls `Game.exe` 的绝对鼠标模式改为由适配包的哈希限定原生补丁自动应用，不再要求设备执行 Python 脚本。
- 触屏菜单点击此前有真机证据；连续圆周摇杆、补丁自动应用/恢复及完整持续游玩仍需按当前版本重新验收。

## 新仙剑的两个目标

### `NewPAL_Release.exe` 2.17.102.0

- 使用 `wine-nx-probe/profiles/newpal-steam/`。
- 这是较新的程序，与 2001 年 `Palgame.exe` 的 DirectDraw/Bink/Miles 路径不同，不能混用结论或配置。
- 已撤回会造成画面重叠嫌疑的居中方案；相关真机结论以该目录 README 为准。

### 2001 年 `Palgame.exe`

目标程序：

- 根目录 `Palgame.exe`，32 位 i386 Windows GUI。
- SHA-256：`6c78208f89dc83a13a49512d783e3b6c02ad6768cfa82f67d914c1b8afa7794c`。
- `Data/Palgame.exe` 是另一份 2004 年程序，不属于当前适配目标。
- 适配目录：`wine-nx-probe/profiles/newpalxp/`。
- 当前适配包版本 1，`min_api` 5。

当前配置：

```ini
title=新仙剑奇侠传（2001）
d3d=wine
wined3d-frontbuffer-swap=1
own-controls=1
controller=keyboard
verbose=0
windows=compositor
window-fit=1
locale=zh_CN.UTF-8
```

已经确认的现象：

- 启动弹窗原先五个中文选项显示为问号；启用 `zh_CN.UTF-8` 后中文能正常显示。
- 游戏目录自带的 `DDraw.dll` 是 DDrawCompat。它在原 Windows 环境的日志里能完成 hooks，但在 Switch/Wine-NX 上会在 GL 初始化/挂钩阶段失败，出现 `wined3d_adapter_gl_init Failed to get a GL context`，随后从空函数指针崩溃。因此设备上的本地 DDrawCompat 必须保持停用，使用 Wine 内置 `ddraw.dll`。
- 纯 `wined3d-renderer=gdi` 能让部分游戏画面出现，但开场视频无声且帧率不可接受，已经撤回。
- GPU 路径下 Bink 视频可以正常显示；经过音频后端修改后，已出现视频声音和游戏内音效同时工作的实机结果。
- 最近一次已确认的核心失败仍是：视频自然播放结束后黑屏；按 B / Esc 跳过时停在视频最后一帧或黑屏，但游戏内音效继续，说明进程和游戏逻辑仍在运行。
- 用户允许为了缩短测试时间直接按 B；自然播放结束仍需在候选修复稳定后单独复测。

DirectDraw 证据：

- 定向日志中的 `[NXDDRAW] update` 在 Bink 阶段持续增长，约到 480 次，`interval=0`。
- 按 B 后 Bink 线程退出，程序继续读取游戏数据，但旧测试版没有新的 `[NXDDRAW]` 更新。
- 静态反汇编显示正式游戏的帧提交函数约在 `0x453b40`：先 `IsLost`/`Restore`，再 `WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN)`，最后 `Flip(DDFLIP_WAIT)`；另一分支使用 `Blt(DDBLT_WAIT)`。
- 因而视频阶段主要通过 Lock/Unlock 和主表面更新提交，正式游戏阶段转入 Flip/Blt。只修视频的前缓冲提交不足以修复正式游戏画面。

当前源码实验：

- `runtime.c` 只在配置 `wined3d-frontbuffer-swap=1` 时向 `WINE_D3D_CONFIG` 添加 `nx_frontbuffer_swap=1`。
- `ddraw_surface_update_frontbuffer()` 在该开关下把主表面更新 blit 到 swapchain back buffer，再显式 present；这条路径已让视频正常提交。
- 最新源码又在 `IDirectDrawSurface::Flip()` 前遍历 flip chain，把默认 CPU 可映射副本标记为 authoritative/dirty，处理旧游戏在 Unlock 后继续通过保留指针写显存、WineD3D 却沿用旧 GPU 副本的情况。
- 该最新 `Flip` 脏区版本已完成定向编译并通过 OpenMTP 覆盖到设备的 `drive_c/windows/syswow64/ddraw.dll`；本地测试 DLL SHA-256 为 `91fada5d0b5a52c86b5af093c083f0573f1440d7da49654415592190926d39b1`。
- 用户在要求整理文档前尚未回报这一个最新版本的实机结果。因此它是“已上传、未验收”，不能写成已修复。

下一步：

1. 完全退出游戏后重新启动，开始新游戏并直接按 B。
2. 等待数秒让游戏资源加载，观察是否从视频进入正式游戏画面、帧率是否正常、视频/游戏音频是否都存在。
3. 若仍失败，立即下载本轮新的 `game-Palgame.log`，确认时间/摘要与旧日志不同。
4. 检查新日志是否出现 `interval=1` 的 `[NXDDRAW]` 或 Flip 后更新。若 Flip 未出现，继续跟踪正式游戏的 Blt 分支；若 Flip 出现但画面仍旧，检查 dirty subresource 和 present 的源/目标表面关系。
5. B 跳过路径通过后，再单独验证视频自然结束。

## 仙剑奇侠传三当前状态

- 目标为 `PAL3/PAL3.exe`，适配目录为 `wine-nx-probe/profiles/pal3/`；适配包不分发游戏本体，包含经真机文件核对的 PAL3patch 5.1 三份文件。
- 本地原游戏随附 PAL3patch 2.1；真机 DLL 为 5.1，`config.ini` 改回 `motionblur=1` 后与本机逐字节一致。
- 真机已验证跳过黑色覆盖层后场景可见、Box64 x87 修复后剧情走到地震、视频居中，且视频结束后剧情与声音继续。
- `pal3-black-overlay-skip=1` 与 `pal3-movie-center=1` 是独立的 PAL3 专用开关；Box64 FNSAVE/FRSTOR 转换是共享指令语义修复，保留对应回归测试。
- `nolockablebackbuffer=0` 保持视频流畅；`profile=1` 在唯一确认片尾正常的真机组合中，暂保留于 PAL3 配置，作用尚需单变量验证。
- PAL3 配置包直接覆盖 `PAL3patch.conf`、`PAL3patch.dll`、`PAL3.dll`，并纳入配置包备份、回滚和恢复；游戏的 `config.ini` 不带入包。启动器设置、按键、作弊和封面仍走原合并流程；当前配置包要求运行时 API 10。最初偶发启动崩溃未确认单独修复。
- 当前源码的 NRO、D3D9 和 WineD3D DLL 已定向构建并上传真机，读回哈希一致；用户重新测试后确认仙剑三目前运行无问题。配置包安装和恢复流程尚未在真机完整验证。详情见 `wine-nx-probe/profiles/pal3/README.zh-CN.md`。

## 不应提交的本地证据

- 原始游戏目录和任何 `Palgame.exe`、数据文件、存档。
- 从设备下载的 `game-Palgame.log`、`stderr.txt`、`wine-nx-runtime.log`。
- 临时 `ddraw.dll`、`wined3d.dll`、NRO 和 OpenMTP 中转目录。
- DDrawCompat 覆盖目录、失败转储和本地构建缓存。

这些文件可用于诊断，但 Git 里只保留源码、测试、配置、封面和不含游戏内容的说明。
