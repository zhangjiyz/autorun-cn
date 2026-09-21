# 分游戏适配包：一张管理表，按需更新

维护者只编辑 [`catalog.json`](catalog.json) 这一张游戏映射表，每个游戏仍有自己的配置、按键、可选金手指和封面文件。发布工具自动生成一张在线管理表 `autorun-profiles.tsv`，以及每游戏一个独立 ZIP。TSV 是生成物，不需要手动维护第二张表。

## 玩家操作

主程序 **设置 → 系统 → 适配包管理**：

- **管理表更新地址**：输入完整 HTTPS 文件地址，也可指向自己的 CNB Release 或静态文件；留空使用安装包内置表。
- **立即更新管理表**：只获取名字、版本、包下载地址和校验值，不下载任何游戏适配包。
- **启动游戏前自动更新适配包**：默认开启，仅检查已经手动绑定的游戏。可关闭；检查或下载时按 B 跳过更新继续启动。

单游戏 **游戏配置 → 常规 → 适配包更新**：

1. 首次在“选择 / 更换适配包”按名称、关键词筛选，核对适用版本后确认。
2. 只下载选中的那个游戏 ZIP，并记录管理表地址、游戏 ID 和版本。
3. “检查此游戏适配包更新”只查这个 ID；相同版本或旧版本不会重装。
4. “恢复上次配置”恢复上一次安装前的配置、按键、金手指、封面和绑定。若希望持续使用旧版，应先关闭自动更新。

启动器启动及转发器/命令行的原生 SD 路径启动，都会在读取游戏配置前执行自动检查；同一次启动的启动器和运行时交接不会重复请求。没有绑定、自动更新关闭、来源不同或 USB 游戏不发起自动更新。不同游戏互不影响。

自动检查管理表的请求限时 5 秒，包请求限时 10 秒，进度回调约束整个检查/下载过程约 12 秒。网络、校验或取消导致失败时继续使用旧配置；只有无法恢复的本地事务才阻止启动。当前仍只支持 SD 卡上的游戏。

成功取得的管理表按完整来源地址缓存。新地址不会使用旧地址的缓存；默认来源可回退到内置表。已有同哈希的内置游戏 ZIP 可直接应用，省去网络下载。自定义源无法连网且没有匹配缓存时，列表不可用，游戏仍能使用原配置启动。

## 唯一维护表

`catalog.json` 的根字段为 `schema: 2`、`profiles: [...]`，每个条目：

| 字段 | 用途 |
| --- | --- |
| `id` | 稳定游戏/版本 ID，仅小写字母、数字、横线，不要复用或改名 |
| `name` | 列表显示名称，建议含适用游戏版本 |
| `version` | 正整数包版本；测试打包保持不变，仅维护者明确要求时手动递增 |
| `min_api` | 包能力级别，目前为 2，对应金手指及封面框架 |
| `keywords` | 中文、繁体、英文、拼音等筛选别名 |
| `description` | 适用版本、前提和更新说明 |
| `settings` | 默认 `.wine-nx.txt` 的相对路径 |
| `keys` | 默认 `.keys.txt` 的相对路径 |
| `cheats` | 可选，金手指 JSON 相对路径 |
| `cover` | 可选，PNG 封面相对路径 |
| `url` | 可选，此游戏 ZIP 的完整 HTTPS 下载地址；未填则自动使用指定仓库/标签 |

默认表中有 NewPAL 2.17.102.0、赵云传 2002ls。赵云传适配包使用 API 3 的声明式二进制补丁：主程序只修改用户选中的、SHA-256 完全匹配的 `Game.exe`，先保存原文件并支持恢复。它不执行包内脚本，也不包含游戏本体和存档。`patch-game.py` 仅作为电脑端手动工具保留。

同一游戏的不同版本配置不兼容时使用不同 ID。首次应用或更换管理表会应用包列出的默认值；同来源、同 ID 更新使用旧默认值/本地值/新默认值的三方合并，保留玩家修改和删除的配置。金手指迁移规则见下文。

适配配置可以包含 `title`，首次应用后作为游戏库显示名；玩家随后手动修改的名称在同一适配包更新或同版本重装时会保留。

## 生成与发布

从工程根目录运行：

```sh
# 本地 CI：只做适配包检查和发布文件；不会上传
./local-ci.sh profiles

# 直接生成，建议输出到专用发布目录
python3 wine-nx-probe/tools/package-profiles.py --output-dir dist/profiles
```

输出示例：

```text
autorun-profiles.tsv
profile-newpal-steam-v1.zip
profile-newpalxp-v1.zip
profile-zhaoyunzhuan-v1.zip
```

每个 ZIP 内只有对应游戏的 manifest、settings、keys，以及可选的 cheats 和 cover。每个包独立版本、独立 SHA-256，不再发布聚合 `autorun-profiles.zip`。旧聚合包只保留读取兼容与回归测试，新的在线列表不会下载它。

生成的管理表包含 ID、显示名、版本、最低 API、关键词、说明、下载 URL、SHA-256、字节数。最多 128 个游戏；单 ZIP 最大 16 MiB，单配置/金手指定义小于 8 KiB，封面最大 2 MiB 和 2048×2048 像素。包下载后依次检查长度、SHA-256、ZIP 内容和条目 ID/版本/API；包与表不符不安装。

主程序读取 CNB `PalmMuse/autorun-cn` 最新正式 Release 的 `autorun.zip`。适配表默认地址：

```text
https://cnb.cool/PalmMuse/autorun-cn/-/releases/latest/download/autorun-profiles.tsv
```

发布同一个 Release 时上传 `autorun.zip`（如更新主程序）、`autorun-profiles.tsv` 和表中所有游戏 ZIP。默认使用 latest 下载路径，所以每个最新 Release 都应包含表及引用的各游戏包；即使只更新一个游戏，也要保持其他条目的下载地址可访问。可用条目 `url` 指向保留的旧 Release 附件，避免重复上传未变的包。

也可独立使用固定标签：

```sh
python3 wine-nx-probe/tools/package-profiles.py --output-dir dist/profiles \
  --repository PalmMuse/autorun-cn --release-tag profiles
```

此时把主程序内管理表地址设为 `https://cnb.cool/PalmMuse/autorun-cn/-/releases/download/profiles/autorun-profiles.tsv`。先上传所有游戏包，最后更新表；不得改变已有版本号包的内容。SHA-256 和大小由本地 ZIP 自动计算，不依赖 CNB 元数据。

## 默认地址与配置迁移

主程序默认地址在 `source/autorun_update.h` 的 `AUTORUN_PROFILE_INDEX_URL` 中，可在源码修改；运行时设置优先。`switch/wine/profile-updates.txt`：

```ini
index-url=https://cnb.cool/PalmMuse/autorun-cn/-/releases/latest/download/autorun-profiles.tsv
auto-update=1
```

`index-url=` 为空表示离线；`auto-update=0` 关闭自动更新。完整安装包只内置同一管理表，不附带所有游戏 ZIP。若需离线首次应用，把选定游戏的独立 ZIP 手动放入 `switch/wine/profiles/`，文件名保持不变。`package-autorun.py --profile-repository` 与 `--profile-release-tag` 会生成对应表地址和包地址。

v21-v23 的 `repository/tag` 设置会被读为对应 CNB 仓库的管理表地址，明确的空仓库继续表示离线。原 GitHub 默认管理表地址会自动迁移到当前 CNB 构建地址，用户填写的自定义地址会保留。旧在线绑定记录的是 owner/repo，新绑定记录完整表 URL：旧在线绑定需手动重新选择一次；自动检查不会静默迁移来源。旧离线绑定可继续匹配离线列表。

更换管理表地址后，已有游戏同样需要手动重新选择一次才迁移。恢复备份可能恢复旧来源，之后仍按这个规则处理。安装或更新会覆盖包内指定的封面，但不会删除独立下载的原图片。

## 金手指框架

每款游戏在 **游戏配置 → 常规 → 金手指** 管理总开关和条目；进入条目可开关，整数项支持左右步进、A 输入、Y 恢复默认值。修改立即保存到该 EXE 的侧文件，关闭总开关保留逐项选择。

**v22 只提供定义、菜单、保存和运行时回调接口，尚无任何游戏的实际金手指效果。** 菜单明确显示“当前仅保存设置，游戏效果尚未接入”。NewPAL 和赵云传当前的 `cheats.json` 为有效空列表，不冒充已有可用金手指；测试示例不进入正式映射表。游戏内效果需要后续针对具体版本实现并验证后端。

定义格式见 [`examples/cheats.json`](examples/cheats.json)，不要直接当作游戏金手指发布：

```json
{
  "schema": 1,
  "cheats": [
    {
      "id": "example-value",
      "name": "测试数值",
      "description": "仅展示数值设置，不对游戏产生效果",
      "type": "integer",
      "min": 0,
      "max": 100,
      "step": 5,
      "default": 10,
      "backend": "example.value"
    }
  ]
}
```

`type` 支持 `toggle` 和 `integer`。开关项不填写 `min/max/step/default`。整数值使用有符号 32 位整数；步长必须为正，默认值和玩家数值必须满足范围与步长。每包最多 32 项；ID 与 backend 只允许小写英文字母、数字、点、横线和下划线。ID 是稳定选择标识，不能复用给不同效果。

所有条目首次安装默认关闭，`default` 仅是整数初值，不是启用状态。同包更新仅保留 ID、类型、backend 一致且数值仍合法的设置；新增项关闭，范围失效的旧项关闭并回到默认值，已移除的项清理。更换包或来源会重置总开关和条目。空列表会关闭总开关。修改开关不会覆盖最近一次适配包安装的撤销备份。

运行时从目标游戏读取这些侧文件，再经 `game_cheats_dispatch()` 调用明确注册的 `game_cheat_apply` 回调。目前回调为 NULL，只记录 `[CHEATS] ... applied=0 ... (framework only)`；不会执行下载脚本、代码或任意内存写入。后续后端需验证游戏版本、决定何时应用/撤销效果，并补充设备测试。单游戏设置不等于运行中悬浮菜单。

## 封面

把 PNG 放入该游戏配置目录，例如 `newpal-steam/cover.png`，在对应 catalog 条目增加 `"cover": "newpal-steam/cover.png"`，然后重新打包。测试时保持现有 `version`；维护者明确要求发布新版本时才递增。建议竖版 2:3 封面；启动器按各展示区域缩放裁切。未声明时无需占位图。

玩家应用选中的适配包后，封面以该 EXE 独立路径安装，自动设置为当前封面并立即刷新，不必重启启动器。同目录不同 EXE 不共享该文件。所有条目的资源虽在同一下载包中，只安装玩家选中的那个条目。

之后通过 SteamGridDB 手动下载图片可覆盖当前显示选择；下一次应用带封面的适配包会重新使用包内封面。包移除 cover 声明时会移除其托管封面并回到原有图片逻辑，不删除玩家之前下载的图片。“恢复上次配置”会同时恢复封面及显示选择。打包器验证 PNG 分块、CRC、尺寸和体积，设备再完整解码后才允许安装。

## 本地状态与恢复

以 `Game.exe` 为例，除现有的 `Game.wine-nx.txt` 和 `Game.keys.txt` 外，安装器维护：

```text
Game.wine-nx.txt.adaptation         包 ID、名称、版本、仓库、标签
Game.wine-nx.txt.profile-settings   上次应用的默认配置
Game.wine-nx.txt.profile-keys       上次应用的默认按键
Game.wine-nx.txt.cheats             此包的金手指定义
Game.wine-nx.txt.cheat-options      该 EXE 的总开关、逐项开关与数值
Game.wine-nx.txt.profile-cover.png  此包的封面
Game.wine-nx.txt.profile-backup     最近一次操作前的完整快照
Game.wine-nx.txt.profile-pending    未提交事务的回滚记录，仅安装期间存在
```

快照按实际内容长度存储，包含 CRC，并兼容 v21 的五文件快照；恢复旧快照时新增的可选文件视为不存在。

这些侧文件跟随游戏保存，不应作为发布默认配置上传。只写从启动器所选游戏路径推导出的固定文件，不使用压缩包提供的目标路径。不触及游戏 EXE、DLL、存档、资源或其他游戏。

提交前写出并同步八个目标文件的旧快照（含二进制封面）。安装失败立即回滚；安装途中退出后，下次启动器读取此游戏设置会先恢复未完成事务。若备份损坏或存储无法恢复，会阻止继续启动该游戏并提示错误，避免用混合版本配置运行。应保留 `profile-pending` 和 `profile-backup`，修复存储问题后重试。

## 验证

```sh
python3 wine-nx-probe/tests/check-game-profiles.py
python3 wine-nx-probe/tests/check_local_ci.py
```

主机测试需要 C 编译器、pkg-config、minizip、curl、OpenSSL、SDL2、SDL2_ttf、libpng。
覆盖管理表解析、分游戏打包、来源隔离、按需下载、哈希与版本校验、配置合并、金手指保存、二进制封面恢复、提交中断恢复，以及手动选择和启动前更新流程。
Switch 编译、主机测试与实际设备验收是不同层次；发布后还需确认真实 HTTPS 下载、网络不可用时继续启动、物理按键和存储恢复。
