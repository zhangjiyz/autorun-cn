# 本地 CI：主程序与游戏配置包

在工程根目录执行，无需 CNB Token，不提交代码、不创建 Release、不上传文件。

```sh
# 主程序 + 一张管理表 + 每游戏独立 ZIP
./local-ci.sh all

# 测试构建：把该轮测试 Release 地址写入主程序包的 profile-updates.txt
./local-ci.sh all --build-type Debug --profile-tag profile-test-001

# 仅检查、打包游戏配置，不编译运行时
./local-ci.sh profiles

# 仅运行主机回归检查
./local-ci.sh check

# 查看计划，不构建、不下载、不修改文件
./local-ci.sh all --plan
```

## 环境与首次运行

宿主需要 Python 3.9 或更新版本、Git，以及已经启动的 Docker。
当前构建容器使用 Linux ARM64，适合本工程使用的 Apple Silicon Mac；
x86 主机需要 Docker 配置 ARM64 模拟执行，速度会明显下降。

测试、编译器和打包工具安装在 Docker 镜像中，不依赖宿主的 Homebrew 包。
首次运行自动构建所需镜像并下载依赖，需要网络和较多磁盘空间。
`profiles` / `check` 只准备较小的测试镜像；`all` 准备 Switch/LLVM-MinGW 构建镜像。

完整构建使用固定 devkitPro 镜像摘要、LLVM-MinGW 20260505（下载后验证 SHA-256），
以及源码中固定的 Box64、DXVK、VKD3D、libusbhsfs、LSFG 版本。
`ci/Dockerfile` 中的 apt 依赖仍跟随发行版仓库，不能宣称跨时间逐字节可复现。

Mesa 固定为 `c68987266979dc7b4105877f2bf27543e76a1622`。
本地已有该版本的源码和完整 SDK 时直接复用，并在 `BUILD.json` 记录静态库哈希；
否则自动获取源码、构建 Mesa 镜像和 SDK。发现其他版本或已修改的 Mesa 源码会停止，
不会重置它。需要强制重编 SDK 时运行：

```sh
./local-ci.sh all --rebuild-mesa --jobs 4
```

`--jobs` 默认 4，限制主程序、Wine DLL 和图形组件构建并发。
Mesa 沿用上游 Ninja 并发策略；首次构建建议给 Docker 分配至少 12 GB 内存。
首次完整编译可能需要较长时间。后续构建复用 Docker 层和构建目录。

## 输出与日志

每次运行创建独立目录，不覆盖之前的成功产物：

```text
dist/local-ci/时间戳-随机后缀/
  build.log                  所有步骤的完整输出
  SUCCESS                    全部要求的步骤完成后才创建
  release/
    autorun.zip              仅 all 模式：中文主程序及公共运行时
    autorun-profiles.tsv      all / profiles：唯一在线管理表
    profile-游戏ID-v版本.zip  all / profiles：每个游戏独立适配包
    BUILD.json               源码、工具链、步骤及每个游戏包的版本/地址/哈希/长度
    SHA256SUMS               ZIP、TSV、BUILD.json、RELEASE.md 的 SHA-256
    RELEASE.md               可用作 CNB Release 说明的文本
```

`check` 模式只输出检查记录及说明，没有 ZIP。失败留下 `FAILED`、`build.log`
和可能存在的 `pending/`，不会生成 `SUCCESS` 和 `release/`。
运行时会打印实际日志位置，可另开终端 `tail -f /实际路径/build.log` 查看。

也可通过 `--output /一个尚不存在的目录` 指定此次运行目录；应放在工程外或被 Git 忽略的目录下。
上传前可在 `release/` 中执行 `shasum -a 256 -c SHA256SUMS`。

允许构建未提交的本地修改，`BUILD.json` 会记录提交、工作区状态和文件内容哈希。
构建期间若源码改变，整轮结果会标记失败，避免发布来源不一致的产物。
正式发布前应将对应源码一并提交，尤其不要漏掉新增的适配目录、CI 和测试文件。

构建缓存放在 `wine-nx-probe/build-local-ci/`，与此前 macOS 原生 Wine PE 构建目录分开。
配置打包沿用已有脚本的临时 staging 目录；不要同时运行另一套主程序打包。
本入口通过 `build-local-ci/pipeline.lock/` 阻止自身并发运行。
如果被强制结束导致锁残留，确认没有构建运行后，才移除这个空锁目录。

## 配置包更新来源

主程序和适配包默认使用 CNB 仓库 `PalmMuse/autorun-cn`，统一维护于
`wine-nx-probe/source/autorun_update.h` 的 `AUTORUN_DEFAULT_REPOSITORY`。
`Release` 的主程序更新和适配包管理表默认都指向最新正式 Release；`Debug` 的两项更新默认都指向
`profile-debug` 标签。`--profile-tag` 可覆盖适配包标签；Debug 主程序同时使用该标签，Release 主程序始终使用最新正式 Release。
适配包管理表地址同时编入 NRO，并写入安装包的
`switch/wine/profile-updates.txt`；安装包本身不会上传这个配置文件。

```sh
# 与主程序一起放在最新正式 Release
./local-ci.sh all --profile-repository PalmMuse/autorun-cn

# 使用固定测试标签；主程序和适配包都从该标签更新
./local-ci.sh all --build-type Debug --profile-tag profile-test-001

# 不填写网络更新源；离线使用需另外拷贝所需游戏 ZIP
./local-ci.sh all --profile-repository ''
```

一张卡切换测试和发布时，安装对应 `autorun.zip` 后启动主程序即可。
主程序在线更新会保留卡上的 `profile-updates.txt`；新主程序读取时，如果当前地址仍是上次构建写入的默认地址，
会将它改为本次 Debug/Release 构建的地址，同时保留 `auto-update` 开关。
通过主程序在线更新时，你在菜单里手动改过的自定义地址会保留；手动解压整个 ZIP 到卡上会用包内文件覆盖卡上的同名配置文件。
旧包没有构建地址标记时，已知的 GitHub 正式版/Debug 默认地址也会迁移到当前 CNB 构建地址；用户手动填写的自定义地址仍会保留。
主程序在线更新器严格选择 `autorun.zip`。Release 构建读取 CNB `PalmMuse/autorun-cn` 最新正式 Release；
Debug 构建读取 `--profile-tag` 指定的准确标签，并允许该 Release 标为预发布。适配功能读取同一轮构建指定的
`autorun-profiles.tsv`，按表中 URL 只下载对应游戏 ZIP。主程序和适配包使用独立缓存和安装流程。
`--profile-repository` 只覆盖适配包来源；主程序仍从 CNB `PalmMuse/autorun-cn` 读取。`--profile-tag` 在 Debug
构建中同时控制主程序和适配包更新标签，Release 构建的主程序仍只使用最新正式 Release。
空仓库参数会将主程序内的 `index-url` 留空；仍生成管理表及独立 ZIP，
表中默认下载地址使用项目仓库。离线安装时将所需 ZIP 拷入 `switch/wine/profiles/`。
在线主程序默认写入 `auto-update=1`；用户可在“设置 → 系统 → 适配包管理”中
手动更改管理表 URL、刷新管理表或关闭启动游戏前自动更新。
已有设备需要先手动安装一次包含此修改的 Debug 主程序，之后才能通过固定测试标签更新主程序。

## 手动上传 CNB Release

完整发布先上传 `autorun.zip`、表中所有游戏 ZIP，确认附件可下载后，最后上传 `autorun-profiles.tsv`；
同时上传 `SHA256SUMS`、`BUILD.json`。
`RELEASE.md` 可粘贴为说明。`autorun.zip` 含 NRO、配套 Wine DLL、32/64 位 DXVK、
64 位 VKD3D、许可证及管理表，不携带游戏本体或所有游戏适配 ZIP。

测试打包时保持 `profiles/catalog.json` 中各条目的 `version` 不变；每次 CI 都会生成独立产物目录。
只有维护者明确要求加版本号时，才手动递增对应条目的 `version`。
只调整游戏配置时可执行 `./local-ci.sh profiles`，上传各游戏 ZIP，最后上传新的 `autorun-profiles.tsv`。
已绑定游戏的启动前自动更新只安装更高版本；在单游戏菜单中手动选择适配包时，允许重新安装同版本。
因此重复测试适配包可保持版本号和测试 Release 标签不变；替换该标签下的游戏 ZIP 和管理表后，手动重选即可：

```sh
./local-ci.sh profiles --profile-tag profile-test-001
```

同一张卡上测试主程序时，使用 `./local-ci.sh all --build-type Debug --profile-tag profile-test-001`。
将该轮所有游戏 ZIP、`autorun.zip` 和 `autorun-profiles.tsv` 上传至 `profile-test-001` 标签的测试 Release（管理表最后上传），
测试 Release 可以标为预发布。第一次需手动安装这次生成的 `autorun.zip`；启动后，主程序更新和管理表都会固定读取这个标签。
以后可删除并替换同标签下的附件：主程序更新页会一直提供手动安装，单游戏“适配包更新 → 选择 / 更换适配包”也允许重装同版本。
Debug 不会因为固定测试标签在每次启动时弹出主程序更新提醒。
发布时执行 `./local-ci.sh all --build-type Release`，安装正式 `autorun.zip` 后地址自动切回正式来源。
来源切换后，已绑定游戏不会自动跨来源更新；需要在单游戏菜单中重新选择对应来源的适配包。
更换来源会按首次应用处理并备份原配置；建议在测试游戏副本上操作。
正式发布的附件不要覆盖；固定测试标签可按需替换附件。不要把测试 Release 设为 Latest。
所有游戏集中列在这一张管理表中，各游戏 ZIP 独立维护和比较版本；不需要重编主程序。
游戏版本和最低运行时能力 `min_api` 的约定见[配置包维护说明](../wine-nx-probe/profiles/README.zh-CN.md)。

管理表名 `autorun-profiles.tsv` 不能改（自定义表 URL 除外）。配置更新器校验表中记录的每包 SHA-256 和长度；
主程序更新校验 CNB 元数据中的附件 SHA-256；适配包校验使用管理表记录的哈希。

若采用固定 `profiles` 标签，手动将配置 ZIP 更新到该 Release，并避免将它设为主程序的 Latest。
若固定标签下的旧附件不能直接覆盖，先删除同名附件再上传，并在最后更新管理表。
若采用空标签（latest），每个最新正式 Release 都需要携带管理表及表中使用 latest 地址的游戏 ZIP。
不要把仅有配置的 Release 当作完整主程序发行版。

公共发布应保留包内第三方许可证，并提供对应源码。LSFG 的固定源码版本和 Horizon 补丁见
[`wine-nx-probe/lsfg/README.md`](../wine-nx-probe/lsfg/README.md)；CI 不下载或分发 `Lossless.dll`。

## 验证边界

流水线运行已有适配包事务、恶意 ZIP 拒绝、配置合并/恢复、CNB 附件筛选和菜单回归测试，
以及 DXVK 架构、哈希、许可证和 AMD64 DLL 依赖打包检查。
完整构建额外校验图形能力要求、NRO 头、包内完整文件清单、文件 SHA-256、源码提交、
32/64 位 DXVK 存在性，以及内置管理表和发布管理表逐字节一致、更新地址与 CI 参数一致。

`all` 和 `profiles` 都检查管理表格式、重复 ID、HTTPS 下载地址、版本、API、SHA-256 和长度，
逐个核对 ZIP 内只有对应游戏且元数据与表一致，拒绝缺失、多余、重复或越界文件。
发布目录不能混入旧版聚合包或未被管理表引用的 ZIP；主程序包也不能夹带游戏适配 ZIP。
任一检查失败都不会生成成功发布目录。

CI 通过只代表这些构建和主机检查通过。Switch 上的显示、输入、音效、性能及剧情流程仍需实机验收。
