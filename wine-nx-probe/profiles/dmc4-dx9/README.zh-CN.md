# 鬼泣4（原版 DX9）

入口为游戏根目录的 `DevilMayCry4_DX9.exe`。2026-09-26 用户确认当前真机配置可玩；本包以本轮重新读取的配置和按键为基准，不包含游戏本体、存档或 DLL。

本包要求运行时 API 14。应用时完整替换 `C:\users\steamuser\AppData\Local\CAPCOM\DEVILMAYCRY4\config.ini`，目录缺失时创建；原文件随适配包一起备份，可通过“恢复上次配置”还原。整文件替换会覆盖已有画质、声音、线程和手柄选项；更新时同样直接替换。

收录文件为 782 字节，SHA-256：`1084e27184f548911f005308385f4527af4a9c5a6ac3c9e14bf608347a0ec816`。画质与性能选项沿用 2026-09-26 20:44 重新读取的真机配置：1280×720、纹理和光照 MEDIUM、三线性过滤、特效质量及数量 LOW，关闭 SLI、动态模糊、法线贴图、高光、深度纹理及阴影。保留 `[CPU] JobThread=1`、`RenderingThread=OFF`，Autorun 使用 DXVK、32 位地址空间、Compositor、`own-controls=1`、`verbose=0`。

按用户要求在 Autorun 按键映射中交换 A/B、X/Y：`A=0x200`、`B=0x201`、`X=0x202`、`Y=0x203` 分别发送手柄 A/B/X/Y。这些映射直接作用于 XInput，按键选择列表中显示为“手柄 A/B/X/Y”。游戏 `[JOYPAD]` 恢复为原来的 `A=2`、`B=1`、`X=3`、`Y=0`，由按键映射统一负责交换。

当前设备日志实际加载游戏目录自带的 `DXVK v3.1-17-g878473ba`；本包不指定独立 DXVK 版本、不移动本地 d3d9.dll，也不自动添加 DLL。系统运行时应使用已补齐 AVIFIL32.dll 的全量组件包。

封面为 600×900 PNG，来自 [LaunchBox 鬼泣4 图片库](https://gamesdb.launchbox-app.com/games/images/3687-devil-may-cry-4)，[原图](https://images.launchbox-app.com/c0c3b1bb-052b-444c-b9d5-2a48a0fb9055.jpg)转换为 PNG 后原样收录；应用适配包时设为游戏库封面。

本次本地包检查和文件替换/恢复主机测试不代替新包的真机应用及恢复验收。
