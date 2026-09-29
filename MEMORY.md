# AutoRunNX 当前工程状态

更新时间：2026-09-29。Biko3 状态已整理为保留版；下方其他游戏及发布基线保留原日期的历史语境，不代表已经重新核对。此文件不把尚未完成的真机实验写成已支持。

## Biko3 / 尾行3：2026-09-29 保留版与清理结果

- 当前分支为 `test_wx3`；目标是设备 `/switch/wine/drive_c/Biko3/Biko_DVD.exe`，SHA-256 `7c02023d9acda74b16431eea2d0c4e610df35e6fa1802bcbfe8a78994923a495`，不是 `biko3.EXE`。原游戏 EXE 未修改。
- 颜色及离屏窗口居中修复已在提交 `1fab115`。设备游戏配置为 `verbose=0`、`locale=ja_JP.UTF-8`、`wined3d-csmt=0`、`window-fit=1`、`d7vk-offscreen-opengl=1`。
- DDraw 保留三项用户选定的优化：不透明排序计划缓存、带原函数抽查和回退的保守墙面远距跳过、两个调用者均弃用返回值的三角形可见性块跳过。实现源码已纳入 `third_party/dxvk-sarek-biko3/`；详细测量、适用边界和构建哈希见 `docs/biko3-optimizations.md`。
- 2026-09-29 的精简版已移除 Biko 采样、QPC 计时、实验统计和日志；`ddraw.dll` 已上传设备并读回核对 SHA-256 `15002e10bb2821bc3cf087e8dac5679a8dcde62dac9a54eacb84d6645b9f2231`。原版 D3D9 保持 SHA-256 `43555a32bdf6e3509461c4761012cb38bd8b636025ca879115f685cea9738eb6`。精简版尚未重新启动游戏，不能把读回当成运行验收。
- 旧 Biko 诊断代码、DLL 备份和日志已按用户要求清理；其他游戏的文件保留。没有纳入 D3D9 状态去重、视图逆矩阵缓存、额外视线跳过、CSMT 等未见收益或失败的实验。
- 低帧问题仍未解决。过去同场景多轮大致在 17–19 FPS；排序缓存曾在一轮测得约 16.1 → 17.85 FPS，墙面跳过的同视角顺序对照约 17.90 → 18.10 FPS，可见性块无明确体感收益。视频闪退按用户要求暂缓，音频、存档、重启和持续游玩未在本轮完整验收。

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
