# 新仙剑奇侠传 NewPAL：实机验证用配置

适用于 `NewPAL_Release.exe` 2.17.102.0，SHA-256：
`1301e72bf46008eb74a0f49ab81d9894ab92d037d63c63d86c906aa067c961dc`。

将本目录的两个 `.txt` 文件放在游戏 EXE 旁边；先备份已有配置。
设置文件保留此次实机原有的 `d3d=dxvk`，并启用独立按键。
需要运行时 `nx-amd64-box64-18` 或更新版本：`controller=keyboard`
隐藏 XInput 手柄并强制保留键盘/鼠标映射，避免 SDL 的手柄探测抢走输入。
其他游戏不设置此选项时沿用原生手柄自动接管行为。

| Switch 控件 | 发送给游戏的输入 |
|---|---|
| 左摇杆、十字键 | 方向键 |
| A / B | Enter / Esc |
| X / Y | 空格 / Ctrl |
| L / R | Page Up / Page Down |
| + / − | Enter / Esc |
| 右摇杆 | 鼠标移动 |
| ZR、右摇杆按下 / ZL | 鼠标左键 / 右键 |
| 左摇杆按下 | Shift |
| 触屏 | 鼠标定位及左键 |

## 2026-09-20 诊断依据与验证边界

实机为 `nx-amd64-box64-16`、32 位地址空间。原配置只有 `d3d=dxvk`，
没有游戏专用按键。日志记录 SDL 窗口 `rect=0,0-902,697`，右摇杆可移动鼠标，
加载 D3D9/OpenGL 并将屏幕交给 OpenGL surface；后续按不区分大小写检索发现 XINPUT9_1_0 加载记录；仅凭加载不能认定为 XInput 接管。

曾尝试 `aspect-fit=896x672` 和 `touch-coordinates=game`，用户报告画面
出现重叠，当前配置已撤销这两个选项。现有居中代码假定 WineD3D 已在
左侧生成完整的 960×720 画面，该假设未在 NewPAL 上得到验证，不应继续套用。
画面可能恢复左偏，正确居中仍待适配。

这是一份待实机验收的配置，不代表已解决画面或输入问题。启动后检查：
左右黑边、菜单鼠标点击、进入游戏后的方向移动、Enter/Esc，以及触屏落点。
用户已确认其他平台的同版游戏主菜单支持方向键，主菜单键盘操作也属于验收范围。
首次排查曾开启 `verbose=1`。用户随后报告声音卡顿、菜单迟迟不出现，
因此本地配置已恢复 `verbose=0`，先排除详细日志的额外开销；关闭日志
并不等于媒体问题已解决。关闭详细日志的更新已通过 MTP 部署并读回校验一致。
若调整了游戏分辨率，应重新检查 `aspect-fit` 和鼠标映射。

两份配置已通过 MTP 部署到 `sdmc:/switch/wine/drive_c/NewPAL/`，
并从设备重新下载，与本目录文件逐字节比较一致。部署前配置和日志备份在
工作区外层 `diagnostics/newpal-20260920-225734/`。游戏重启后的显示、输入
验证尚未完成。

## 音频与菜单等待：第二次日志

`diagnostics/newpal-verbose-audio-20260920-230920/` 保存开启跟踪后的日志。
游戏日志约 34 MiB、612398 行，其中 604819 行为 `[SYSCALL]`，
因此全量跟踪是必须先排除的额外开销。
VLC 报告 `ES_OUT_SET_(GROUP_)PCR is called too late`，
延迟依次升到 300、462、669、1441、2200 ms，并重复重置 PCR。
同时出现 libmad 的 `bad main_data_begin pointer` 和 `Huffman data overrun`。
这些证明媒体播放异常，但尚不能确定是解码、调度、数据读取还是其他原因。
本次只有 67 次插件加载记录，不能继续把全部启动等待归因于首轮 316 个插件扫描。

这轮只把 `verbose=1` 改回 `verbose=0`，保留显示、按键和媒体组件。
下一次应在关闭跟踪的条件下对比菜单等待及听感，再决定音频改动。

## v17 强制键盘与显示回退

用户报告画面重叠，并确认此游戏主菜单在其他平台可用方向键。
最新低日志开销记录仍显示 24 项按键映射已经读取；重复写相同按键文件
无法排除原生手柄检测对映射的抑制。因此新增 `controller=keyboard`：
XInput 状态、震动和由状态派生的能力查询均表现为未连接；运行时也不因
最近一次 XInput 轮询而清空映射按键。默认模式及其他游戏保持原行为。
该选项是输入路径修正，尚不能单凭静态测试断定实机按键问题已解决。

v17 NRO 与当前四行配置均已 MTP 部署、读回 SHA-256 校验一致。
NRO SHA-256：`4cd0e6bfbc904e3027a2abed3b12a83da08cd2807ab614a6cf2f256429cd6a88`。
旧 v16 NRO、构建产物及部署前静默日志保存在工作区外层
`diagnostics/newpal-keyboard-v17/`。
XInput 原生/WoW64 查询、强制键盘下禁用轮询、恢复默认模式，以及摇杆
回归测试均通过 UBSan；Switch 增量构建和 `git diff --check` 通过。
实机需重新启动后验证主菜单方向键、A 确认、B 返回和画面重叠。

## v18：窗口呈现、SDL 音频与有限按键诊断

v17 实机日志确认新运行时和强制键盘模式已经生效，但用户仍报告画面在
左下角、按键无效；背景音乐正常，只有游戏音效没有声音。

- `window-fit=1`：让 win32u 为游戏建立真实客户端尺寸的离屏帧缓冲，
  从该帧缓冲等比缩放到 1280×720 正中，并清除黑边。跳过旧的
  `aspect-fit` 屏幕搬移；触屏使用同一矩形逆映射，再加客户端桌面原点。
  `[NXWINDOW]` 只在尺寸或原点变化时记录实际源尺寸和目标区域。
- `sdl-audio=directsound`：在此游戏的 Windows 进程环境中加入
  `SDL_AUDIODRIVER=directsound`。当前原生音频后端只允许一个播放客户端，
  SDL 与 VLC 分别打开客户端可能冲突；DirectSound 可在进程内共享混音设备。
  这是待实机验证的音频调整，尚未证实 VLC 的实际输出模块或冲突返回值。
- 输入保持 v17 键盘映射，新增 `[NXKEY]` 发送、服务端队列、取消息记录，
  各限制前 48 条；`[NXAUDIO]` 最多记录 32 次播放客户端创建及返回值。
  保持 `verbose=0`，不启用全量系统调用跟踪。

音频后端 UBSan 回归覆盖播放、格式转换和第二个客户端被拒绝；Horizon
键码/扫描码/修饰键/原始输入测试通过，Switch 增量构建通过。
这些检查不代表实际显示、SDL 菜单输入或音效已验收。
v18 NRO SHA-256：
`b5e301a57ef4dddd128f5333026e6382e210c3fb4e8cffa3358727ee97b0f592`。
备份与构建文件在工作区外层 `diagnostics/newpal-window-audio-v18/`。

复测：重启 Autorun 并启动 NewPAL；主菜单按上、下、A、B 各一次；
检查画面居中和鼠标落点；进入游戏触发一次明确的音效后退出并打开 MTP。
日志能区分按键未生成、无焦点、未被取出及音频设备创建失败，不能直接
证明 SDL 或游戏逻辑已消费事件。

v18 运行时及六行游戏配置均已通过 MTP 部署，并重新下载逐字节校验一致。
实机复测待用户完成。

## v19：对照 RGWinRunner 后修复硬件消息的队列分类

用户在 v18 报告方向键仍无效，按菜单键后长时间才显示菜单。
对照实际 RGWinRunner 工程：`keymaps/newpal.gptk` 的方向/Enter/Esc 映射
与此配置一致；`lib/common.sh` 启动 gptokeyb2，通过 `/dev/uinput` 发送
`EV_KEY` 和 `SYN_REPORT`，进入 Linux 输入系统与完整 Wine。该工程还使用
NewPAL 原生媒体桥和 wineserver64；不能直接将 Linux 配置搬到 Switch。

在 Switch 的 `horizon_server_refresh_queue_locked()` 中确认独立缺陷：
所有非 WM_MOUSEMOVE 的硬件消息都被统计为 QS_MOUSEBUTTON，包括键盘和
WM_INPUT。键盘入队时设置的 QS_KEY 会被随后的 `horizon_msgq_update()` 清除，
导致 GetQueueStatus / 只等待键盘的消息循环看不到正确类型的待处理输入。
现在按 `server/queue.c` 的分类方式区分键盘、原始输入、鼠标移动和鼠标按钮。

`tests/horizon_msg_queue.c` 新增组合回归：入队、刷新、等待、保留消息、移除消息，
以及键盘/原始输入/鼠标混合队列。保留旧分类实现时断言失败，修复后通过 UBSan；
原有键码、修饰键、原始输入测试也通过。保留 v18 的有限事件日志以便实机验证。
这是已证实的兼容层缺陷，但尚未凭 v18 实机日志证明它是 NewPAL 所有输入
问题及菜单卡顿的唯一原因。MTP 未连接时不替换当前配置，先取回本轮日志。

v19 Switch 构建通过，NRO SHA-256：`05c92b8ba66eb6503d66984671c065b865557f3b6c68493ee0746045c1438e2e`。
构建文件保存在 `diagnostics/newpal-keyboard-queue-v19/`，尚未部署。

## v20：v18 实机日志确认扫描码缺失与文件信息查询停顿

日志取回于 2026-09-20 23:56，运行标记是 v18；v19 当时尚未部署。
日志保存在 `diagnostics/newpal-log-check-20260920-235608/`，另有 v20 目录中的备份。

**输入证据：** 主窗口 00010036、线程 0004 实际取走了 WM_KEYDOWN/UP。
例如 VK_DOWN 的 scan=50、flags=0、lParam=00500001，VK_UP 的 scan=48、
flags=0、lParam=00480001。独立方向键必须带 E0 前缀与 lParam 的扩展位。
`NtUserMapVirtualKeyEx()` 的 null-driver US 表先匹配到小键盘别名，返回
裸扫描码；所以“输入完全未到达窗口”的推测不适用于这次日志。
v20 对控制器映射中的方向、PageUp/Down、Home/End、Insert/Delete 指定独立
导航区 E0 扫描码，Enter/Esc、普通键和真正的小键盘键保持原映射。
保留 v19 的队列分类修正，但该修正本身不足以解决此次缺少扩展标志的问题。

**停顿证据：** 126、136、146、156 秒的 frames 都是 3794，gl_frames 都是3783；
期间 cache_hits 从69694升到103930。根据运行时模块基址20c8a000，重建出与
设备 NRO SHA-256 完全一致的 v18，并保存对应 ELF，解析主线程 4w：
`NtQueryInformationFile -> fd_get_file_info -> fsdev_fstat -> fsdev_converttimetoutc -> svcSendSyncRequest`。
启动时的长停顿也采样到相同调用链。不能据此认定菜单停顿的全部时间都花在
这条链上，但它是连续多次采样命中的实际阻塞点。

新增仅此游戏启用的 `sd-stat-cache=1`，复用 SD 字节缓存的只读文件生命周期：
成功的 fstat 结果按打开句柄缓存；写打开、重命名、删除、截断使同路径缓存
失效；失败查询不缓存，未知/可写句柄直接查底层。外部同时改写文件不在该
模式的支持范围内，与现有只读数据缓存一致。默认关闭，其他游戏不启用。
`[PROGRESS]` 的 stat_queries/stat_hits 用于下轮比较实际底层查询数和命中数。

UBSan 回归通过：实际扫描码 helper 接 Horizon 事件生成，验证导航键按下/
释放的 E0 和 lParam、普通 Enter/Esc 与真小键盘8；消息队列分类；文件信息
一万次查询只读取一次、失败不缓存、写入失效、路径失效、句柄复用及未知句柄。
Switch 构建通过，NRO SHA-256：
`52ae253a7c73fbc5f8a7e4aaf3c308fb85b74c859f785f1a0b8d5cbaa9dc1dd1`。
构建和旧版备份：`diagnostics/newpal-scancode-stat-v20/`；包含 v20 ELF，便于精确解析后续栈。

v20 NRO 及七行配置已通过 MTP 部署，设备读回与构建/配置逐字节一致。
键盘响应与菜单等待时长仍待用户实机复测，未将构建/传输成功视为验收通过。
