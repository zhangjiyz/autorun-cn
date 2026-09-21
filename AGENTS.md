# AutoRunNX 协作约定

本文件适用于仓库根目录及全部子目录。目标是让后续接手者基于真实代码、构建产物和 Switch 实机证据继续工作，不把静态检查或启动成功误写成兼容完成。

## 仓库与分支

- 实际 Git 仓库是当前 `autorun-cn/` 目录；外层 `AutoRunNX/` 只是工作目录。
- 日常开发分支是 `main_cn`。
- `origin` 是 GitHub fork；`cnb` 是 `https://cnb.cool/PalmMuse/autorun-cn.git`。
- 只有用户明确要求时才提交或推送。推送前必须检查分支、远端、完整 diff、暂存范围和上游差异。
- 工作区可能同时包含多轮设备适配，不能随意恢复、覆盖或删除不属于当前任务的改动。
- 不提交游戏本体、ROM、用户存档、设备日志、临时 DLL、构建目录、下载缓存或 `.DS_Store`。

## 工作方式

- 用户主要使用中文，进度、诊断和交付说明使用中文。
- 先追踪真实调用链和现有配置，再做最窄的修正；不要凭兼容层常识猜测原游戏行为。
- 用户会手动跑完整 CI。日常适配只运行与改动直接相关的定向编译、主机测试或静态检查，不要每次都跑完整 CI。
- 定向构建通过只说明相应目标能编译；主机测试通过只说明主机回归；二者都不能代替 Switch 真机验收。
- 真机结论至少区分：启动、画面、帧率、视频、音乐、音效、输入、触屏、存档/读档、退出/重启和持续运行。
- 遇到失败先拿新日志并确认它来自本轮启动。比较大小、时间和摘要，避免重复分析旧日志。
- MTP 覆盖系统 DLL 后必须完全退出并重新启动游戏，确保新进程重新加载 DLL。

## 主要代码位置

- `wine-nx-probe/source/launcher*.c`：中文启动器、库界面、更新和适配包菜单。
- `wine-nx-probe/source/runtime.c`：每游戏配置、输入、窗口合成、WineD3D 环境和运行期日志。
- `wine-nx-probe/source/audio_unix.c`：Switch `audout` 共享音频后端。
- `wine-nx-probe/source/game_profiles.c`：适配包校验、应用、回滚和哈希限定的二进制补丁。
- `wine-nx-probe/profiles/catalog.json`：适配包映射；当前 schema 为 3，运行时 API 为 5。
- `dlls/ddraw/surface.c`：新仙剑 2001 的 Wine DirectDraw 前缓冲实验。
- `wine-nx-probe/tools/`、`ci/build-runtime.sh`、`docs/local-ci.md`：本地检查、打包和 CNB Release 流程。

## 配置和发布约定

- 默认仓库为 CNB `PalmMuse/autorun-cn`。
- Release 构建读取最新正式 Release；Debug 构建使用固定测试标签，默认 `profile-debug`，也可由 `--profile-tag` 指定。
- 正式发布时才按维护者要求递增适配包版本。固定测试标签允许替换同版本包；手动选择适配包允许重装同版本，自动更新仍只接受更高版本。
- 适配包先上传独立游戏 ZIP，最后上传 `autorun-profiles.tsv`。不要把测试 Release 设成 Latest。
- NACP 版本采用“上游三段 + 本分支修订号”，当前源码为 `0.0.1.1`。
- 原生二进制补丁必须绑定精确 SHA-256、在修改前备份，并能由适配包恢复流程还原；版本不匹配时保持游戏文件不变。

## 新仙剑 2001 当前边界

- 目标是根目录的 32 位 `Palgame.exe`，不是 `Data/Palgame.exe`，也不是 `NewPAL_Release.exe`。
- 使用 `wine-nx-probe/profiles/newpalxp/`；设备上游戏目录自带的 DDrawCompat 必须保持停用。
- `wined3d-frontbuffer-swap=1` 是单游戏开关，不能变成所有游戏默认行为。
- Bink 视频阶段与正式游戏阶段使用不同的 DirectDraw 提交路径。分析时同时检查 Lock/Unlock、主表面更新、Flip 和 Blt，不能只看视频能否播放。
- 当前详细证据、最近一次失败、已上传但尚未验证的实验和下一步见根目录 `MEMORY.md`。

## 提交前最低检查

1. `git diff --check`。
2. 校验改动过的 JSON、Python 和 shell 文件的基本语法。
3. 对改动模块运行定向测试；若未运行完整 CI，提交说明和交付说明必须明确写出。
4. 复核 `git diff --cached --stat` 与 `git diff --cached --name-status`，确认没有游戏本体、日志或构建产物。
5. 提交后检查工作区剩余内容，再推送明确指定的远端和分支。
