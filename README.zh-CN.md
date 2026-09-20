<p align="center">
  <img src="documentation/logo-image.png" alt="Autorun 标志" width="220">
</p>

<h1 align="center">Autorun</h1>

<p align="center">
  在 Nintendo Switch 上玩 Windows PC 游戏。<br>
  <sub>原名 Wine-NX。</sub>
</p>

Autorun 是一款在 Switch 上运行 Windows 游戏和程序的自制软件。它把让 Windows 软件在其他系统上运行的 [Wine](https://www.winehq.org) 带到 Switch 自身的操作系统，并通过 [Box64](https://github.com/ptitSeb/box64) 将游戏的 PC（x86）代码转换为 Switch 的 ARM 处理器可运行的代码。游戏需要你自行准备：将自己拥有的 PC 版游戏复制到 SD 卡，再从 Autorun 的游戏库启动。

> **Autorun 仍处于实验阶段。**有些游戏运行良好，更多游戏会在启动后遇到尚未实现的功能而退出。每次运行都会生成日志，记录运行停止的位置。

![Autorun 启动器](documentation/launcher.jpg)

## 运行情况

以下结果来自上游项目的真实 Switch 测试；当前汉化构建仍需实机复测：

| 游戏 | 运行情况 |
|---|---|
| Halo: Combat Evolved | 可以游玩，约 30 fps。需要[32 位转发器](#需要-32-位转发器的游戏)。即使不超频也能使用最高画质。 |
| WarCraft III | 游戏内约 24–34 fps，菜单约 50 fps；开场影片可以播放。首次游玩前需运行一次设置程序。 |
| Need for Speed Underground 2 | 有声音且支持手柄的比赛约 20–45 fps。需要 32 位转发器。不超频可在低画质游玩，超频后可使用最高画质。需要 [xinput 模组](https://github.com/xan1242/NFSU-XtendedInput)。 |
| Need for Speed: Most Wanted | 不超频可在低画质游玩，超频后可使用最高画质。需要 [xinput 模组](https://github.com/xan1242/NFS-XtendedInput)。 |
| Left 4 Dead 2 | 可以进入游戏，但负载很重，仍有优化空间。Autorun 只运行单个进程，无法运行 Steam，因此需要配合 Goldberg Steam 模拟器；请使用你自己的原版游戏副本。 |
| Fallout: New Vegas | 可以进入游戏，但负载很重，仍有优化空间。 |
| OpenTTD | 有声音，最高可达 60 fps。 |
| Quake III Arena (Quake3e) | 720p 下约 37 fps。 |

仍在研究：**The Sims 2 Legacy Collection** 目前无法运行，它在加载标题画面时退出。**Ultimate Collection** 尚未测试。

7-Zip 和记事本等程序也可以运行。未列出的程序能否运行尚不确定；可以尝试运行，若程序退出，日志会记录缺少的内容。

## 准备工作

- 一台安装了 Atmosphère 自定义固件、可以运行自制软件的 Nintendo Switch。
- 一张有足够空间存放 Autorun 和游戏的 microSD 卡。
- 已安装的 PC 版游戏文件夹。当前源码包含 **32 位 x86** 和实验性的 **64 位 x86-64** 运行路径；具体游戏能否运行仍需逐一测试。

## 安装

1. 下载 Autorun 的 `.zip` 安装包。
2. 将压缩包解压到 SD 卡的**根目录**，文件会放入 `switch/wine`。
3. 在自制程序菜单中进入 **wine** 文件夹，选择 **Autorun**（文件名为 `wine-nx-runtime.nro`）。

游戏需要较多内存。请从游戏中打开自制程序菜单（启动游戏时按住 **R**），而非从相册打开。也可以在 **设置 → 系统 → 制作 Autorun 转发器** 中将 Autorun 加入 HOME 菜单，并从那里启动。

## 游戏适配包

在游戏配置的“常规 → 适配包更新”中，首次按名称或关键词筛选并选择适配包；应用后记住绑定，后续只检查这个适配包的更新。应用前备份配置，更新时保留玩家后来修改的值，也可恢复上次配置。目前支持 SD 卡游戏。适配包可携带封面，应用后自动显示；单游戏“金手指”菜单支持总开关、逐项开关和数值保存，目前是框架，具体游戏效果尚未接入。

维护者编辑 [适配映射表](wine-nx-probe/profiles/catalog.json)，通过 [适配包发布说明](wine-nx-probe/profiles/README.zh-CN.md) 生成 GitHub Release 附件。主程序使用本项目最新正式 Release 的 `autorun.zip`；适配功能使用一张 `autorun-profiles.tsv` 管理表和每游戏一个独立 ZIP，只下载选中的游戏包。主程序“设置 → 系统 → 适配包管理”可修改管理表地址、手动更新表，或开启启动前自动更新已绑定游戏。游戏本体需自行准备。

## 添加游戏

1. 将游戏文件夹复制到 SD 卡的 `switch/wine/drive_c`。这个文件夹是游戏看到的 `C:` 盘。
2. 在 Autorun 中按 **+ → 添加游戏**，选择游戏的 `.exe` 文件。
3. 按 **A** 开始游玩。

添加游戏时会默认启用 DXVK，它通过 Switch 的 Vulkan 驱动绘制 Direct3D 游戏。如果希望新游戏继续使用 WineD3D，请关闭 **设置 → 新游戏默认使用 DXVK**。

通用汉化包不附带游戏或游戏专用设置程序。若游戏需要安装过程、注册表项或额外运行库，需要针对该游戏检查并补齐。

### 游戏预期已经存在的设置

有些游戏会读取通常由自身安装程序或启动器生成的设置文件，缺少这些文件便拒绝启动。游戏会把 SD 卡的 `switch/wine/drive_c/users/wine/Documents` 视为 Windows 的“文档”文件夹；通用汉化包不会替特定游戏生成设置文件。

**Fallout: New Vegas** 就是其中之一。缺少设置时，它可能无法识别显卡，转交给 `FalloutNVLauncher.exe` 后退出。上游的游戏示例包曾附带 `My Games\FalloutNV\FalloutPrefs.ini`；使用通用汉化包时，需要从你自己的游戏安装或启动流程生成相应设置。

### 需要 32 位转发器的游戏

一些老游戏只能加载在固定的低地址内存区域，《极品飞车：地下狂飙 2》和 Halo 就是例子。Autorun 检测到此需求后会提示设置转发器：**设置 → 系统 → 制作 32 位转发器** 会在 HOME 菜单添加“Autorun 32 位”图标。请从这个图标启动这类游戏。

## 使用启动器

| 按键 | 作用 |
|---|---|
| **A** | 启动选中的游戏 |
| **Y** | 打开游戏设置 |
| **L / R** | 在主页（最近游玩）和游戏库之间切换 |
| **−** | 在主页打开设置；在游戏库打开筛选和排序 |
| **+** | 添加游戏，或退出 Autorun |

各界面也支持触摸操作。

**游戏设置（Y）**可收藏或隐藏游戏、更改标题、设置命令行参数、下载封面、选择图形渲染方式（通过 Wine 或 DXVK 运行 Direct3D 9），以及设置游戏专用按键。

**设置（−）**可显示隐藏的游戏、设置所有游戏默认使用的按键、填写用于下载封面的 [SteamGridDB](https://www.steamgriddb.com) API 密钥、设置游戏结束后返回 Autorun、制作转发器，以及查看鸣谢。

## 游玩与操作

默认情况下，手柄充当鼠标和键盘：

| 操作 | 发送的输入 |
|---|---|
| 右摇杆或触摸屏 | 移动鼠标指针 |
| **A** / **B** | 鼠标左键 / 右键 |
| 十字键、左摇杆 | 方向键 |
| **+** | Esc |
| **X**、**Y** | 空格、F |
| **L**、**R** | Tab、Shift |

每个按键都可以更改：在 **设置 → 游戏默认设置 → 按键设置** 中更改所有游戏的默认按键，或在某个游戏的 **游戏设置（Y）→ 按键设置** 中单独设置。两个摇杆、十字键和触摸屏都可以用来移动鼠标，或发送方向键、W A S D。支持手柄的游戏会识别到 Xbox 360 手柄。

### 《三国赵云传》示例配置

安装包的 `switch/wine/profiles/zhaoyun` 下有这款游戏的专用按键、鼠标模式补丁脚本和操作说明。将 `Game.keys.txt` 复制到你自己的 `Game.exe` 旁边，并在该游戏的 `Game.wine-nx.txt` 中设置 `own-controls=1`、`left-stick-run=shift`、`left-stick-eight-way=1`、`left-stick-aim=140` 和 `left-stick-move=mouse`。默认左摇杆按实际角度沿圆周连续定位鼠标并按住左键移动，按住 L3 为键盘步行；默认速度取决于游戏底部的“走／跑”按钮。右摇杆左/上/右切换弓/剑/枪，十字键与 L/ZL/R/ZR 对应道具快捷栏 1–8；A 发送空格，B 发送鼠标右键，X 发送 Ctrl，Y 发送 C。触屏定位和菜单点击此前已在 Switch 上确认；连续圆周摇杆方案尚待实机验证，完整说明见配置旁的文档。

如果游戏以 800×600 运行、画面靠左，可以在同一份 `Game.wine-nx.txt` 中设置 `aspect-fit=800x600`。这会将 WineD3D 已放大的 960×720 画面居中。针对这份《三国赵云传》2002ls 可执行文件，再设置 `touch-coordinates=screen`，并按配置旁的说明修改你自己的 `Game.exe` 鼠标模式，触屏就会按 Switch 屏幕位置定位。其他图形路径仍需逐一验证。

《新仙剑奇侠传》NewPAL 2.17.102.0 的实机验证配置见 [NewPAL 配置说明](wine-nx-probe/profiles/newpal/README.zh-CN.md)，包含独立键盘映射和 `controller=keyboard` 强制键盘模式。造成重叠嫌疑的居中设置已撤回。

**同时按住 + 和 − 一秒钟**可关闭游戏。

## 遇到问题时

每次运行都会在 SD 卡的 `switch/wine` 中留下两份日志：

- `wine-nx-runtime.log`：最近一次运行的日志。
- `game-NAME.log`：该游戏最近一次运行的专用日志。

反馈问题时，请附上游戏日志并描述看到的现象。在游戏设置中开启**详细日志**可获得更多信息，但会降低运行速度。

## 限制

- 速度：对硬件要求较高的 3D 游戏主要受游戏代码实时转换速度限制，图形芯片通常不是主要瓶颈。
- 内存：游戏最多可使用约 2 GB。
- 一次只能运行一个游戏，且只支持一个手柄。
- 游戏可能需要 SD 卡上尚不存在的 Windows 文件；日志会指出缺少的文件。

## 开发者资料

本地一键检查和发布打包：`./local-ci.sh all` 生成 `autorun.zip`、一张 `autorun-profiles.tsv` 管理表和每游戏独立 ZIP；`./local-ci.sh profiles` 只生成管理表和游戏适配包。CI 校验管理表与每个包的版本、内容、哈希和长度，主程序只内置管理表。产物和校验文件保存在 `dist/local-ci/`，手动上传 GitHub Release 时先上传 ZIP，最后上传管理表。环境、参数及发布说明见[本地 CI 文档](docs/local-ci.md)。

Autorun 的工作方式、构建流程、测试和文件布局见[技术文档](documentation/technical.md)。更新说明另见[中文文档](wine-nx-probe/UPDATING.zh-CN.md)。

## 鸣谢

Autorun 建立在以下项目之上；各项目保留自己的版权和许可证。

| 项目 | 许可证 | 用途 |
|---|---|---|
| [Wine](https://www.winehq.org) | LGPL-2.1-or-later | Windows API、加载器和图形层；本仓库是 Wine 的分支 |
| [Box64](https://github.com/ptitSeb/box64)（ptitSeb 和贡献者） | MIT | 运行 32 位 x86 代码 |
| [DXVK](https://github.com/doitsujin/dxvk) | zlib/libpng | 通过 Vulkan 运行 Direct3D 9 |
| [Mesa](https://mesa3d.org) 和 [mesa-switch](https://github.com/danfromtico/mesa-switch) | 主要为 MIT | Switch GPU 上的 OpenGL 和 Vulkan |
| [libnx](https://github.com/switchbrew/libnx) 和 [devkitPro](https://devkitpro.org) | ISC；各组件许可证不同 | Switch 系统库和工具链 |
| [SDL2、SDL2_ttf](https://www.libsdl.org)、[FreeType](https://freetype.org)、[HarfBuzz](https://harfbuzz.github.io)、[libpng](http://www.libpng.org)、[zlib](https://zlib.net) | zlib、FTL、MIT、libpng | 启动器 |
| [dolphin-nx](https://github.com/NaGaa95/dolphin-nx)（NaGaa95） | GPL-2.0-or-later | 启动器的外观；代码由 Autorun 自行实现 |

包含作者及参考项目的完整列表见[技术文档的鸣谢章节](documentation/technical.md#credits)。
