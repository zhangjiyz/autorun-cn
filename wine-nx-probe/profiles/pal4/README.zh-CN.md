# 仙剑奇侠传四（语音版）首版配置

目标是游戏根目录的 32 位 `PAL4.exe`。本地样本 SHA-256 为 `ed168442c54f1f48332dcb25986c3de85a64c5aef423cdcc61aae2845bb8beee`。它直接依赖 `PAL4Extend.dll`、`PAL4P.dll`、D3D9、DirectInput、Bink 和 Miles 音频。配置包只携带设置、按键、空金手指列表和封面；游戏本体、扩展 DLL、`config.cfg`、语音资源及存档均由现有游戏目录提供。

封面以用户提供的实体盒封照片为基础，校正了拍摄透视并清理桌面背景与反光，保留“仙剑奇侠传四”标题、四名人物和装饰边框；这是整理后的封面图，不是原盒封的逐像素扫描。

图形配置沿用仙剑三的 WineD3D、窗口合成与等比显示作为首轮测试起点。不带入 `pal3-black-overlay-skip`、`pal3-movie-center`、`wined3d-explicit-buffer-flush=0` 或 `profile=1`；这些值与仙剑三的实机情况相关。游戏本地 `config.cfg` 当前为 1024×768、窗口模式、非宽屏，配置包不改动它。

## 首轮按键

按[官方《仙剑奇侠传四》游戏说明书第 14–15 页](https://store.steampowered.com/manual/1621680)的键盘表设置。语音版的实际按键仍须在 Switch 上确认。

| Switch 控件 | 发送给游戏的输入 | 预期作用 |
| --- | --- | --- |
| 左摇杆、十字键 | 方向键 | 移动、菜单选择；不启用仙剑三的双击奔跑序列 |
| 右摇杆左右 | 逗号 / 句号键（说明书标为 `<` / `>`） | 旋转镜头；需实机确认是否要求 Shift |
| A / B / Y | Space / Esc / Enter | 互动或跳跃 / 系统菜单及返回 / 确认 |
| X | C | 迷宫场景特技 |
| L / R | R / Tab | 切换走跑状态 / 迷宫切换领队 |
| + / - | Esc / M | 系统菜单 / 小地图 |
| ZR / ZL | 鼠标左键 / 右键 | 点击确认 / 返回 |
| 触屏 | 鼠标移动与左键点击 | 界面操作 |

## 验证边界

2026-09-22 已通过 OpenMTP 的 `mtp-cli` 将 `profile-pal4-v1.zip` 放入真机 `switch/wine/profiles/`，并将设置、按键与封面侧文件放入 `switch/wine/drive_c/Pal4/`。四个文件读回后 SHA-256 均与本地一致；真机 `PAL4.exe` 也与上述样本哈希一致。后续须分别核对启动、图形初始化、片头视频、音乐、语音、场景、战斗、按键、触屏、存档和退出。若无法启动，采集本次新日志；目录中 2025 年的旧日志不能代表本次测试结果。

首次真机启动返回 `c0000135`：新生成的 `game-PAL4.log` 明确记录 `CEGUIBase.dll` 依赖的 `MSVCP60.dll` 缺失，后续 `OIRAMLOOK.DLL` 和 `PAL4.exe` 导入失败；游戏目录内的 `pal4Logfile.log` 与 `pal4Exception.log` 仍是 2025 年旧日志。已将用户本机仙剑三外传目录自带的 32 位 `MSVCP60.DLL`（SHA-256 `f1fd0f1a54f196b19a6f21044092c89c02353dad173c236d80f6474cb8a7ea7f`）补入本机 `games/Pal4/` 和真机 `switch/wine/drive_c/Pal4/`，真机读回哈希一致。下一轮已通过 DLL 导入并进入游戏初始化；没有把这个游戏 DLL 放入可分发的配置包。

第二次真机启动已经进入游戏初始化，但长时间黑屏。新 `pal4Logfile.log` 走到 `auAudioMgr initializing begin...`，随后没有旧版日志中的 `initializing 2d driver OK`。同一轮 `game-PAL4.log` 在 25 秒时仍只记录 10 帧，多个 Miles 插件装载返回 `c0000142`，并出现 32 位地址空间映射失败。核对发现设备侧 `PAL4.wine-nx.txt` 已变为 `d3d=dxvk`，与本适配包原设定 `d3d=wine` 不一致；运行日志也确认实际加载 DXVK。已通过 MTP 将设备原配置改名备份为 `PAL4.wine-nx.txt.dxvk-backup-20260922`，仅把 `d3d` 改回 `wine`，保留设备侧的 `profile-cover=1`，上传后读回 SHA-256 一致。此为单变量实验，仍需用户完全退出游戏后重新启动，并根据新日志判断是否通过音频初始化及进入可见画面；不能仅凭 DXVK 与失败同现断言它是唯一原因。

第三次真机启动仍黑屏。新日志确认 `Direct3D Wine`，因此 DXVK 不是此现象的必要条件；日志同样只走到 `auAudioMgr initializing begin...`，随后在扫描 Miles `mss/win32` 插件时发生 Horizon `map_code`、`commit reservation` 失败，插件装载返回 `c0000142`。已在设备上将原来的 35 文件目录改名备份为 `mss/win32.full-backup-20260922`，创建测试用 `mss/win32`，只保留 `Mp3dec.asi`、三个 `Mssv*.asi` 和提供 `Miles Fast 2D Positional Audio` 的 `Mssfast.m3d`。这五个文件上传读回哈希均匹配本地原件。此为可回退的插件隔离实验，尚待完全重启后验证是否进入音频初始化下一步；如果仍失败，要恢复原目录并转向运行时内存映射问题。

第四次真机启动（精简插件实验）仍未显示游戏画面。新 `game-PAL4.log` 在约 25 秒、13 帧后记录游戏主线程 `c0000005` 未处理的读取异常：`EIP=006e5f6e`、读取地址 `160a7a7f`，随后进程自行终止并返回启动器。这次游戏原生日志只到 `CombatTranslation` 表加载完成，未到音频初始化，且运行日志没有尝试装载任何 `mss/win32` 插件。因此精简插件没有解决黑屏，不能将这次异常归因于被移走的具体插件。已将原 35 文件目录恢复为设备上的 `mss/win32`，测试目录保留为 `mss/win32.minimal-test-20260922` 以便核对；下一步应检查游戏主线程异常和运行时的地址映射，而不是继续删减游戏插件。

第五次真机启动使用恢复后的完整 35 文件 Miles 目录，约 60 秒仍黑屏。新日志确认 `Direct3D Wine`，游戏原生日志再次停在 `auAudioMgr initializing begin...`。前几个插件装载成功，从 `Mssdolby.m3d` 起，运行时尝试解除原代码别名的一小段时，`svcUnmapProcessCodeMemory` 返回 `0xce01`，随后预留内存提交失败，多个插件返回 `c0000142`。这说明精简目录实验的更早异常不能代表完整目录的稳定故障点；当前可重复故障是代码映射按保护区域拆分时，元数据边界与内核代码别名边界不一致。

已在 `dlls/ntdll/unix/horizon.c` 实现对应的运行时实验修正：拆分映射元数据前，先把原内核别名完整解除，再按左、中、右区域分别建立别名；失败时尝试恢复原别名。针对精确范围拆分与失败回滚的新主机回归测试，以及原有 Horizon backing/index 测试均通过；32 位地址空间 WoW64、dynarec、WineD3D/LSFG 构建成功。这些检查只证明构建和模拟映射行为，尚未证明真机能进入画面。

测试 NRO 的构建标识是 `nx-wow64-dynarec-227-pal4-maptest`，SHA-256 为 `63c8faeeee064452ecab29dfc54b93b22ede1beedf30c4cc3853aca04ef6075b`。已用 MTP 将原真机 NRO 改名为 `switch/wine/wine-nx-runtime.nro.before-pal4-maptest`，并将新 NRO 放入 `switch/wine/wine-nx-runtime.nro`；上传读回哈希一致。本地原 NRO 备份 SHA-256 为 `af291db196ed93e8c78e6c97bd3857ed9afb63713b4dd7acc8cebebe9a879e32`。设备上的 `build-manifest.json` 仍是旧包清单，记录的 NRO 哈希与实际运行日志及本次覆盖前备份都不一致，不能用它判断本轮运行模式。下一步须完全退出并重新进入启动器，启动仙剑四，观察是否越过音频初始化及出现画面，再采集本轮 `wine-nx-runtime.log`、`game-PAL4.log` 和 `Pal4/pal4Logfile.log`。

第六次真机启动确认测试 NRO 实际运行，但仍黑屏，并且比原 NRO 更早卡住：`pal4Logfile.log` 停在 Monster 表，未进入音频初始化；主线程反复以 `MEM_RESERVE`、`PAGE_NOACCESS` 申请已占用的 `0x19de0000/0x10000`，约 20 秒内 `STATUS_CONFLICTING_ADDRESSES` 计数超过 200 万。之前的部分代码别名解除失败在这一轮未出现，但游戏没有走到会扫描 Miles 插件的阶段，因此不能视为已修复。主机检查还发现本地 `PAL4.exe` 含 `.sforce3` 段及 Protection Technology 字符串；它是加壳样本，当前不能证明加壳是否导致这次地址申请循环。

由于全局映射补丁使 PAL4 更早卡住，已撤回测试 NRO：真机当前 `switch/wine/wine-nx-runtime.nro` 恢复为覆盖前版本，读回 SHA-256 为 `af291db196ed93e8c78e6c97bd3857ed9afb63713b4dd7acc8cebebe9a879e32`。失败测试版保存在真机 `wine-nx-runtime.nro.failed-pal4-maptest`，对应源码 diff 保存在工作目录外的 `diagnostics/pal4-maptest-20260922/runtime-maptest.patch`；源码实验改动也已回退。仙剑四在原 NRO 上的音频初始化黑屏尚未解决，后续实验应先定位受保护可执行文件的固定地址申请和 Miles 插件映射路径，不再使用这版全局补丁。

另一条可回退的音频路径：本地仙剑四的 `Mss32.dll` 是 Miles 6.1a（SHA-256 `d0db426ebac97dc410b5130c9c74d19bc04151236df85b012823d6cb3d7fb87c`），本地仙剑三自带的是 6.1c（SHA-256 `2a515199cc7a2e1ccd8d756efdaab07cedd5c3052309ef1c7499c2560e2097bd`）。静态比对显示，仙剑四版本的 343 个导出函数都包含在仙剑三版本的 351 个导出中；仙剑四本体、扩展 DLL 和 35 个 Miles 插件对 `Mss32.dll` 的命名导入也没有缺项。这只支持做兼容实验，不能保证运行时 ABI 行为相同，因此只替换真机仙剑四目录的主库，保留原 DLL、插件目录、游戏配置和已恢复的原 NRO。

用户重新打开 MTP 后，已核对真机原 `Mss32.dll` 与本地仙剑四原件哈希一致。真机原 DLL 已改名为 `switch/wine/drive_c/Pal4/Mss32.dll.pal4-original-backup-20260922`，仙剑三的 6.1c DLL 已上传至同目录的 `Mss32.dll`；读回 SHA-256 与源文件一致。此轮只改这一个游戏 DLL；必须完全退出并重新启动游戏后，才能判断 Miles 是否越过 `auAudioMgr initializing begin...`，也仍需观察画面、音乐和音效。若失败可将测试 DLL 改名保留，并把原备份改回 `Mss32.dll`。

6.1c DLL 首次真机启动仍黑屏，随后进程以 `c0000005` 退出。新日志确认原 NRO 标识 `nx-wow64-dynarec-227`，但游戏原生日志只到 `CombatTranslation` 表，未到 `auAudioMgr`，运行日志也没有装载 Miles 插件。崩溃点 `EIP=006e5f6e`、调用栈及读取无效指针的形态与此前“精简插件目录”实验一致；两轮的指针高位随地址布局变化。这一轮没有执行到待验证的音频阶段，因此不能据此判断 6.1c 主库是否有效。保留单项改动，重复一次完整启动确认是否能到达音频阶段；若再发生相同的早期崩溃，将恢复仙剑四原 DLL。

6.1c DLL 第二次完整启动仍在 `EIP=006e5f6e` 以 `c0000005` 退出，原生日志仍只到 `CombatTranslation`；两次均未进入 Miles 插件装载或 `auAudioMgr`。相比原 DLL 的完整目录基线能到音频阶段，此替换未带来收益，并有更早崩溃的证据。已将测试 DLL 改名为 `Mss32.dll.failed-pal3-6.1c-test`，把真机仙剑四原版主库恢复为 `Mss32.dll`；读回 SHA-256 `d0db426ebac97dc410b5130c9c74d19bc04151236df85b012823d6cb3d7fb87c`，与本地原件一致。原 NRO、插件目录和配置保持原样。两轮日志分别存于工作目录外的 `diagnostics/pal4-miles61c-20260922/` 与 `diagnostics/pal4-miles61c-retry-20260922/`。

2026-09-23 开始第二次代码别名实验。原全局补丁会在映射元数据拆分时提前改动内核别名，导致更早的固定地址申请循环；这次仅在 `protect_code_mapping()` 的原路径解除旧别名失败时尝试补救。补救先查询内核实际别名的完整范围，确认该范围内所有元数据片段连续、指向同一 backing 的相邻偏移且仍可写，再完整解除旧别名并分别映射片段，失败时回滚原别名。只在 `PAL4.exe` 配置含 `pal4-alias-repair=1` 时启用。定向主机测试覆盖关闭开关、成功拆分和映射失败回滚；Switch WoW64/dynarec NRO 构建通过。此实验仍需真机验证。

测试 NRO 标识 `nx-wow64-dynarec-227-pal4-alias2`，SHA-256 `5525b4cdae55dd4af451d1d844cac45894169b9e19f78a7215e79c5b47f981dd`。上传前真机原 NRO 下载备份并核对 SHA-256 `af291db196ed93e8c78e6c97bd3857ed9afb63713b4dd7acc8cebebe9a879e32`；设备侧改名为 `wine-nx-runtime.nro.before-pal4-alias2`。原 PAL4 配置也保留为 `PAL4.wine-nx.txt.before-pal4-alias2`，测试配置只追加 `pal4-alias-repair=1`。两个上传文件均已从真机读回校验，原版 Miles 主库及 35 个插件保持不变。本轮启动前的三份日志已保存到工作目录外 `diagnostics/pal4-alias2-20260923/`，接下来以新日志和屏幕结果判断是否越过 `auAudioMgr`。

第一次 alias2 真机启动仍停在 `auAudioMgr initializing begin...`。新日志确认构建标识和配置开关生效，但 `Mssdolby.m3d` 的部分代码别名解除仍返回 `0xce01`，未记录一次成功修复；随后它的 attach 返回 `c0000142`，并出现重复的 `c0000005` 内存访问异常。原生日志与运行日志哈希均不同于启动前，确认不是旧日志。需要判断安全条件中哪一项拒绝了此范围，因此只增加拒绝原因记录，保持映射行为和 PAL4 配置不变。诊断版标识 `nx-wow64-dynarec-227-pal4-alias2diag`，SHA-256 `78506e5ef1f62cae470f3d9b41b5471fe1d0c7bcd77b39fb0794e1e98aeb5542`；已从真机读回校验，第一次 alias2 NRO 保存在 `wine-nx-runtime.nro.failed-pal4-alias2`，原版仍在 `wine-nx-runtime.nro.before-pal4-alias2`。诊断版尚待完整重启后采集新日志。

诊断版启动仍在 `Mssdolby.m3d` 报 `0xce01`、attach `c0000142`，原生日志仍止于 `auAudioMgr initializing begin...`。虽然代码已记录权限切换补救被拒绝的原因，本次日志没有任何拒绝或修复记录；实际失败点是 `unmap_range_locked()` 调用 `split_backing_mapping()` 时，局部释放一段尚未拆开的内核代码别名。已将 opt-in 补救接到这条局部释放路径：仅当原局部解除失败时，验证同一 backing 连续覆盖内核原范围，完整解除别名，并重建应保留的片段；中途映射失败时恢复原范围。新增主机测试覆盖局部释放成功、关闭开关和失败回滚，Switch 构建通过。alias3 标识 `nx-wow64-dynarec-227-pal4-alias3`，SHA-256 `ea5058a160d8bda630355c9d59fc80cd40e6434d95dd7703fc4941ec616ef4f4`，已上传并读回一致。诊断版 NRO 保存在设备 `wine-nx-runtime.nro.failed-pal4-alias2diag`，原版仍可从 `wine-nx-runtime.nro.before-pal4-alias2` 恢复。alias3 的真机结果尚待确认。

alias3 首次真机启动仍黑屏。新日志确认运行了 alias3，游戏原生日志进入 `auAudioMgr initializing begin...`，但插件扫描在 `Mssa3d2.m3d` 之后发生重复的 `c0000005` 异常，未走到 `Mssdolby.m3d` 或触发局部别名补救。因此这轮不能判断补救能否解决原故障；已要求再进行一次完整启动，若再次提早异常或补救后仍失败，应撤回设备实验版。

alias3 第二次启动停得更早：`pal4Logfile.log` 只到 `RWCSelectDevice success`，运行日志显示同一线程反复以 `MEM_RESERVE` 申请已冲突的 `0x19510000/0x10000`，返回 `STATUS_CONFLICTING_ADDRESSES`，约 20 秒仍无新画面，也未进入 Miles 插件扫描。两轮均未触发 `PAL4 repaired partial unmap`，不能用它们验证补救是否能解决 `Mssdolby` 故障；重复的早期异常和固定地址循环使 alias3 不适合留在设备。真机原 NRO 和原 PAL4 配置已通过 MTP 恢复并读回校验：NRO SHA-256 为 `af291db196ed93e8c78e6c97bd3857ed9afb63713b4dd7acc8cebebe9a879e32`，配置与本轮测试前逐字节一致，且不含 `pal4-alias-repair`。失败 alias3 NRO 留在设备 `wine-nx-runtime.nro.failed-pal4-alias3`；测试配置留在 `PAL4.wine-nx.txt.failed-pal4-alias3`。实验源码 diff、定向测试和构建目录已移出仓库，保存在工作目录外 `diagnostics/pal4-alias3-20260923/`，仓库运行时代码已回退。此后应优先确定游戏加壳入口对固定地址申请的预期及冲突区域所有者，再决定是否继续修改 Horizon 映射；目前不能称仙剑四已通过实机兼容验证。

2026-09-23 固定地址冲突取证：在 `NtAllocateVirtualMemory` 返回 `STATUS_CONFLICTING_ADDRESSES` 且请求为固定地址 `MEM_RESERVE` 时，PAL4 专用开关 `pal4-va-diagnostic=1` 会记录 Wine 的 `MemoryBasicInformation`（区域状态、类型、分配基址）和 Horizon 的 `svcQueryMemory`（内核区域边界、类型、权限）。每个冲突地址只记录第一次，不改变分配行为。此轮诊断版标识 `nx-wow64-dynarec-227-pal4-vaowner`，NRO SHA-256 `aba5703caa9b7a94f0f0f49537e8a19ab4ce46022e24fbbd47e0ee5f286189a1`，Switch 构建通过，已上传并回读一致。上传前真机 NRO SHA-256 仍为 `af291db196ed93e8c78e6c97bd3857ed9afb63713b4dd7acc8cebebe9a879e32`；设备原 NRO 和 PAL4 配置分别保存在 `.before-pal4-vaowner`，测试配置只追加诊断开关。新日志待完整启动后采集，测试结束须恢复两份原文件。

诊断版首次启动在游戏 `EIP=006e5f6e` 发生 `c0000005`，未重现目标循环。第二次启动重现：`0x1ac40000/0x20000` 的 `MEM_RESERVE` 返回 `STATUS_CONFLICTING_ADDRESSES` 超过 52 万次。Wine 的 `NtQueryVirtualMemory` 报此地址 `MEM_FREE`，而 Horizon 报内核区域 `0x1ac35000/0x16a9000`、类型 `0x8`（ModuleCodeStatic）、权限 `0x5`（RX）。该区域大小与此版 NRO ELF 的 `.text` 按页对齐后的大小完全一致，冲突地址也位于其中，确认被占用的是当前运行时的原生代码段。后来一次用户反馈的采集与第二次三份日志逐字节相同，不能视为新启动证据。

据此增加 PAL4 专用实验开关 `pal4-native-text-guard=1`：初始化 Wine 虚拟内存视图时，查询运行时自身函数所在的内核代码段，并将整段登记为 `VPROT_SYSTEM`，使客体查询及空闲区域搜索避开原生代码。此开关仅在目标为 `PAL4.exe` 时解析，诊断日志开关保留。本版 `nx-wow64-dynarec-227-pal4-textguard` 的定向 Switch 构建通过，NRO SHA-256 `abedb0cc10f0e7a2b47f15dc1a5031c47ef5a42a6c79f42c9bf86b7a24adc810`；真机 NRO 和 PAL4 配置均已上传并回读一致。被替换的诊断版保存为 `.before-pal4-textguard`，最初的原 NRO/配置仍为 `.before-pal4-vaowner`。实验仅针对早期固定地址冲突，Miles 插件阶段的问题尚未验证；待新启动日志和实际画面确认结果。

textguard 真机启动确认记录了 `reserved native runtime text 0xdc094000/0x16aa000`，原来针对 NRO `.text` 的冲突不再出现；但游戏仍在另一个固定地址 `0x1a830000/0x10000` 循环申请，仍未到 Miles 阶段。该范围的前 `0xd000` 字节是内核空闲区域，末尾跨入启动时已有的 `0x1a83d000-0x1a83e000` 只读共享内存页；Wine 仍把整个 64 KiB 报为空闲。故在同一 PAL4 实验开关下，初始化时枚举低 4 GiB 的原生共享内存区域，并按 Windows 的 64 KiB 分配粒度向外取整后登记为 `VPROT_SYSTEM`。`nx-wow64-dynarec-227-pal4-sharedguard` 已完成定向 Switch 构建，真机结果待验证。

sharedguard 真机日志确认两段原生共享内存已登记，上一轮共享页冲突没有再出现，但游戏又在 `0x19b10000/0x10000` 循环申请。启动映射表显示 `0x19b18000-0x19b24000` 是原生线程栈，因此这次申请的后半段跨入线程栈。继续在 PAL4 专用开关下，把启动时低 4 GiB 的 `MemType_MappedMemory`（原生栈）和 `MemType_ThreadLocal` 也按 64 KiB 粒度登记；诊断同时记录失败请求末字节所在内核区域。此轮标识 `nx-wow64-dynarec-227-pal4-nativeguard`，定向 Switch 构建通过，真机结果待验证。

nativeguard NRO SHA-256 `8c91c565fb66878f469d758d6536d59178be63122358354d70d4d90147cd2f2a`，已上传并回读一致；前一实验版保存为 `wine-nx-runtime.nro.failed-pal4-sharedguard`。首次真机启动记录了 7 段低地址原生映射（含栈、线程本地页、共享页）和 NRO 代码段成功登记，没有再出现固定地址的重复 `VAFAIL`。原生日志到达 `auAudioMgr initializing begin...`；运行日志加载并 attach `Mssa3d2.m3d` 后开始重复 `c0000005`，仍无游戏画面。音频阶段问题仍需单独定位；已要求在不修改文件的情况下重复启动，检查其稳定性。

nativeguard 原样复跑却在 `0x23500000/0x10000` 再次出现超过百万次固定地址申请冲突。Horizon 在该地址返回 `MemType_MappedMemory`，而启动时的低地址映射表没有这一段，说明线程栈可在 Wine 初始化后新建；仅扫描启动映射不能稳定避免循环。下一版 `nx-wow64-dynarec-227-pal4-dynamicguard` 保留已有初始登记，并在 PAL4 的固定地址预留失败时，核对请求范围内确有内核占用且 Wine 视图认为相应 64 KiB 块空闲，再补登记一个 `VPROT_SYSTEM` 块；每个冲突地址只尝试一次并记录结果。此版定向 Switch 构建通过，NRO SHA-256 `1d27e8064cd1586b7998c03ff11650fa989a9816bdf9f7cd97ef226d2b95f4ca`，已上传并回读一致；原 nativeguard NRO 保存为 `wine-nx-runtime.nro.failed-pal4-nativeguard`。真机结果待验证。

dynamicguard 这轮没有发生新的固定地址冲突，因此动态补登记分支尚未被实际验证。游戏再次进入 `auAudioMgr initializing begin...`，`Mssa3d2.m3d` 加载时对空闲目标申请 24 KiB 代码别名，多个 `svcMapProcessCodeMemory` 调用返回 `0xce01`（libnx 定义的 `KernelError_ResourceExhausted`），最后 Wine 报 `STATUS_NO_MEMORY`，紧随其后是重复 `c0000005`。另有目标已占用时的 `0xd401`（`KernelError_InvalidMemoryState`），两者不能混为一谈。先前 `Mssdolby.m3d` 的局部解除也曾返回 `0xce01`，同属内核资源耗尽信号。当前证据指向大量低地址代码别名/内存区域在音频阶段消耗 Horizon 映射资源，而不只是可用虚拟地址不足；本轮 Wine 视图约 6270 个，内核仍有数百 MB 连续空闲区。资源上限的具体类别与是否有未释放映射，尚需专门计数验证，不宜继续加入单点地址回避补丁。

上述 PAL4 VA 实验已结束。真机活动 NRO 和 `PAL4.wine-nx.txt` 已恢复为实验前文件，并从设备读回逐字节核对：SHA-256 分别为 `af291db196ed93e8c78e6c97bd3857ed9afb63713b4dd7acc8cebebe9a879e32` 和 `c6509d4e8c5b412c3a78af2c725bbac50d6079c7a77e236db298d8bd9ff83e56`。最后的 dynamicguard NRO/配置分别保存在设备 `.failed-pal4-dynamicguard`；各轮日志、回读文件、源码补丁及 134 MB 构建目录保存在仓库外 `diagnostics/pal4-*-20260923/`。四个运行时代码文件已回退，避免把未完成的实验混入其他游戏。下一步应先测量 PAL4 音频加载前后的内核映射块与代码别名数量，追踪失败前是否有未释放的映射；再决定如何减少映射碎片或调整代码别名策略，而不是继续换 Miles DLL 或增加固定地址补丁。
