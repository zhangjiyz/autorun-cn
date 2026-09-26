# 2026-09-26 上游整合记录

## 范围和决策

- 集成分支：`merge/upstream-20260926`。
- CN 基线：`4b9abcf1fc692bc37ad8364f30de220d91937b92`（`main_cn`）。
- 上游目标：`2ae1be6f740443a0f5fb055a6689ccbb22164778`（本轮拉取的原版 main）。
- 共同祖先：`99de116c87e25f35ce345579ed18e86af494c2f8`；合入范围含 51 个上游独有提交。
- 用户授权一次整合，选择 `1B / 2上游 / 3上游 / 4A / 5上游 / 6A / 7A`。
- 随后用户调整第 1 项：默认采用上游完整 SD 策略，原 CN 策略保留但不开启。
- 策略复核后，用户进一步授权浮动键盘、组件前置注册、游戏目录 DXVK 配置三项全部采用上游默认，并保留关闭开关。
- 本记录对应已解决冲突的工作区；尚未创建合并提交。`main_cn` 保持原基线，未推送、发布或部署。

| 项目 | 实际处理 |
|---|---|
| 1 SD 缓存（追加决策） | 默认完整采用上游读缓存、全局路径 stat／ENOENT 缓存、延迟写入及失效／落盘规则。原 1B CN 后端完整保留，只有显式开启 CN 总开关并重启后才使用。 |
| 2 用户目录 | 完全采用上游 `users/steamuser` 及环境变量、shell 用户文件夹；不新增旧 `users/wine` 数据迁移。 |
| 3 日志与注册表 | 日志写入 `logs/`；hive 写入 `registry/`。保留上游根目录 hive/`.tmp` 迁移逻辑，目标已有 hive 或 `.tmp` 时不覆盖；没有额外 CN 迁移层。 |
| 4 浮动键盘（追加决策） | 键盘及 GL/Vulkan/合成器呈现采用上游；主开关、焦点触发改为默认开启，各自保留关闭开关。保留 CN 鼠标映射、触屏坐标和游戏输入修复。 |
| 5 转发器 | 采用上游 32↔39 双向路由。先恢复适配包、检查更新并重载游戏设置，再判断路由；未配置地址空间时默认 32 位，显式设为“任意”才转交到 39 位。 |
| 6 组件注册（追加决策） | 完整 CN 包包含 `autorun-setup.exe`；按上游条件自动前置注册，默认开启并可关闭。原目标存入 `run-next.txt`，完成标记位于 `registry/components-1.done`。 |
| 7A 内存与翻译器 | 合入 Box64 递归、arena/回收修复，Horizon 线程局部页避让和 PE 共享节映射；保留 CN 独立修复。 |

## 保留与默认值

- 适配包目录内容未改动，API 13、schema 3、通用文件替换 v5 和恢复机制保持原契约。
- 保留仙剑／赵云传相关设置、哈希限定补丁、PAL3 x87 修复、单游戏前缓冲开关；未改成全局默认。
- 保留中文启动器、字体、CNB 更新与 Release 标签、双架构完整组件 CI；用户要求本轮测试包 NACP 更新为 `0.0.1.2`。
- 内部源码标识为 `nx-amd64-box64-28`／`nx-wow64-dynarec-256`。
- 缺失游戏的设置首页直接提供“定位程序文件”和“从游戏库移除”；入口管理绕过适配包恢复，移除后同时刷新游戏库和最近游玩记录，并持久保存。
- FSR 和整数缩放已加入游戏图形菜单，默认 `upscaling=off`。
- 浮动键盘 `floating-keyboard-enabled=true`、焦点触发 `keyboard-on-text-focus=true`、组件自动注册 `setup-components-before-games=true`，由系统设置保存至 `config/settings.json`，均可关闭。
- 游戏目录 `dxvk.conf` 默认读取，系统开关为 `read-game-dxvk-conf=true`；该 EXE 的 `.wine-nx.txt` 可用 `dxvk-use-game-conf=0/1` 覆盖全局默认。文件内容追加在启动器生成的 DXVK 配置之后。
- 保留上游 DLL 子模块引用用于追踪，CN CI 继续从当前源码构建匹配 DLL。
- **设置 → 系统 → SD 缓存使用 CN 策略**按最新测试决策改为默认开启，保存为 `config/settings.json` 中的 `sd-cn-strategy=true`；缺少此项也采用 CN 策略，已有明确保存的值保留。关闭后选择上游策略。开关只在完整退出并重新启动 Autorun 后生效，不在运行中更换文件句柄的后端。

## SD 策略切换

- 可选的上游后端位于 `source/sd_cache_upstream.c`，来自本轮上游目标；仅调整公共符号名称和计数器归属，以接入策略选择层，未改动缓存算法。读缓存头文件独立保留为 `sd_read_cache_upstream.h`。
- 上游具备动态 32–192 MB 读缓存、跨文件 LRU、路径 stat／不存在文件缓存、小写入合并。读取／重新打开、相关路径操作、同步、关闭、截断及按文件末尾定位时按上游规则处理待写数据；运行时每 200 ms 及退出时调用落盘接口。
- 默认的原 1B 后端位于 `source/sd_cache_cn.c`，保留 CN 读缓存及按游戏启用的 fstat／干净写句柄行为。适配包里的 `sd-stat-cache`、`sd-clean-writer-cache` 继续生效；手动关闭 CN 开关使用上游模式时，不启用这些 CN 参数。
- 启动日志显示 `[SDCACHE] policy=upstream installed=1`；CN 模式显示 `policy=cn`。进度日志区分 `sd_stats`／`sd_stat_hits` 和 CN 的 `fstat_queries`／`fstat_hits`，并记录实际 SD 写入、合并写入和各类落盘计数。
- 对比两种策略时，切换系统开关并完全重启，使用相同游戏、存档和操作路径，重点检查加载速度、菜单等待、存读档与退出重启。

## 升级后的目录

- 用户数据：`switch/wine/drive_c/users/steamuser/`。旧 `users/wine/` 不自动移动；新路径可能导致游戏暂时看不到旧存档，需要按游戏将原数据放到新位置。
- 总日志：`switch/wine/logs/autorun_runtime.log`；游戏日志：`NAME.log`，详细／分析运行增加 `_verbose`、`_profiler` 后缀。
- 标准输出：`logs/stdout.txt`、`logs/stderr.txt`；详细 Horizon 跟踪：`logs/horizon-trace.log`。
- 注册表：`switch/wine/registry/system.reg`、`user.reg` 及其 `.tmp`；原位置已有数据会按上游规则移入。

## SD 切换后的策略复核

对照本轮上游目标 `2ae1be6f` 的源文件和共同祖先到上游的修改范围，确认当时还有三处上游默认行为受到 CN 限制。用户选择三项全部切换后，实际处理如下：

| 行为 | 上游 | 当前集成分支 | 关闭方法 |
|---|---|---|---|
| 浮动键盘与文本焦点弹出 | Minus+R3 可直接打开浮动键盘，文本焦点自动弹出默认开启 | 两项默认开启，保留 CN 独立输入修复 | 系统设置关闭“浮动屏幕键盘”，或仅关闭“文本框聚焦时自动打开键盘” |
| Windows 组件前置注册 | 未有完成标记、设置 EXE 存在且可重启时，首次启动游戏前自动运行 | 原实现和 EXE 保留，开关默认开启 | 系统设置关闭“首次运行自动注册组件”；已有 `registry/components-1.done` 则按上游规则跳过，不重复注册 |
| 游戏目录 `dxvk.conf` | DXVK 路径自动尝试读取 EXE 同目录文件，追加到生成配置之后 | 默认自动读取，保留全局默认及单游戏开关 | 系统设置关闭“读取游戏目录 dxvk.conf”；单游戏 `dxvk-use-game-conf=0/1` 优先于全局默认 |

- FSR／整数缩放默认关闭就是上游默认值，不是未合入。需要在单游戏图形菜单选择才能测试效果。
- 用户目录、日志与注册表、32↔39 路由、TLS 页避让、PE 共享节、Box64 递归及 arena／回收修复均已采用本轮上游实现。Horizon 和 Box64 剩余差异是 CN 输入修复／诊断与 PAL3 x87 转换修复；SD 保留两种后端，按最新测试决策默认选择 CN，关闭 CN 开关即可选择本轮上游实现。
- CN 保留的独立实现包括最多 8 路音频混音（上游单渲染流）、按游戏的输入／触屏／窗口与 DirectDraw 修复、适配包及恢复、中文 UI／字体、CNB 更新、双架构源码 DLL 构建。这些不是本轮漏合的上游策略；切回上游会撤去已有功能或适配修复。
- 新增开关没有历史文件可迁移，读取默认值时不应拼接空的旧文件名；本次补齐 `config_bool` 的空值判断。
- 首次使用本轮策略时，已有全局设置备份到 `config/settings.before-upstream-features-v1.json`（文件已存在时不覆盖），一次性将上述四个布尔值设为开启，并写入 `upstream-feature-defaults-v1=true`。这也处理旧构建保存的默认关闭值。后续用户关闭任意开关并保存后，重启不会再强制开启；适配包、游戏配置与 SD 策略开关不参与此迁移。

## 验证

已进行定向构建和主机回归，未运行完整发布 CI：

- Switch AMD64 + WoW64 + Mesa + LSFG + USB NRO 目标增量构建通过。
- `user32.dll`、`win32u.dll` 的 i386／aarch64 目标构建，以及组件注册 EXE 编译通过。
- 适配包打包、恢复、文件替换、金手指框架、中文菜单、CNB 元数据与更新网络回归通过。
- Box64 i386／AMD64 的解释器和 ARM64 dynarec 四项执行测试通过，含本轮新增的递归／回收用例。
- Runtime 主机套件通过：缓存池回收与缩放、CN 写句柄／元数据失效、注册表迁移、键盘、TLS 页避让、映射与匿名共享内存 section、DXVK 依赖与打包等。PE 共享节新增实现已通过编译，未独立验证游戏行为。
- SDL dummy 启动器通过临时运行、库保存、选项、主页交互和 32→39 转发测试；浮动键盘按键、触屏与 720p／1080p 绘制测试通过。
- 主机测试使用 Linux ARM64 GCC 和 ASan/UBSan，保留 x18；GCC 相比原 clang 脚本额外的符号比较／格式截断／同结构字段别名警告未升级为错误。仅依赖 AArch64 Mac 的读重定向测试按原脚本跳过；未暂存的上游游戏 EXE 检查也按原脚本跳过。启动器用已构建的 Notepad PE 作为浏览器样本，不执行该 PE。
- 两份上游 FSR SPIR-V 通过 Vulkan 1.0 `spirv-val`。本地 glslang 12.0.0 重新生成的字节与上游头文件不同，原 `--check` 未通过；保留上游预编译字节，不据此宣称 FSR 实机画面已验证。
- 合并复核中补齐标准类型头文件，修正旧测试的命令行引用预期，并令键盘测试复用字体数据以模拟 Switch 的共享字体服务。
- SD 决策调整后，两套读缓存、上游路径 stat 缓存、延迟写入测试通过 Linux ARM64 GCC ASan/UBSan；策略选择测试覆盖缺省值、CN 旧参数不启用后端、显式关闭／开启，以及运行中配置变化不能更换已安装后端。
- SD 调整后的 `wine-nx-runtime-nro` 定向构建通过，日志为 `sd-upstream-nro.log`；缓存测试日志为 `sd-upstream-tests.log`。未运行全部 CMake 目标、完整发布 CI 或 Switch 实机测试。
- 三项策略切换后，旧配置默认值迁移、关闭后重启保留、未知设置保留、配置容量不足不发生部分写入，以及启动器配置测试通过 Linux ARM64 ASan/UBSan。对应 NRO 定向构建通过；日志为 `upstream-features-tests.log`、`upstream-features-nro.log`。

验证日志位于外层工作目录 `reports/merge-validation-20260926/`。这些结果不证明已适配游戏的 Switch 兼容性；设备上仍需验证启动、画面、音频、输入、存读档、退出重启和持续运行。
