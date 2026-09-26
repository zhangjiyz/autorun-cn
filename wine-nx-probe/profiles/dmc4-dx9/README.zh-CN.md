# 鬼泣4（原版 DX9）

入口为游戏根目录的 `DevilMayCry4_DX9.exe`。2026-09-26 用户确认当前真机配置可玩；本包原样收录本轮重新读取的配置和按键，不包含游戏本体、存档或 DLL。

本包要求运行时 API 13。应用时完整替换 `C:\users\steamuser\AppData\Local\CAPCOM\DEVILMAYCRY4\config.ini`，目录缺失时创建；原文件随适配包一起备份，可通过“恢复上次配置”还原。整文件替换会覆盖已有画质、声音、线程和手柄选项；更新时同样直接替换。

收录文件为 785 字节，SHA-256：`911bf0faaf282193b1b3f23aa1484f8489edec0de5e369a12e9ba805e7992904`。保留 `[CPU] JobThread=1`、`RenderingThread=OFF`，Autorun 使用 DXVK、Compositor、`own-controls=1`、`verbose=0`。用户报告已在游戏里选择最低画质，但读取文件仍为 1280×720、多个 HIGH 字段；本包保存实机文件原文，不把这些字段手动改成 LOW，也不宣称其数值就是最低预设。

当前设备日志实际加载游戏目录自带的 `DXVK v3.1-17-g878473ba`；本包不指定独立 DXVK 版本、不移动本地 d3d9.dll，也不自动添加 DLL。系统运行时应使用已补齐 AVIFIL32.dll 的全量组件包。

本次本地包检查和文件替换/恢复主机测试不代替新包的真机应用及恢复验收。
