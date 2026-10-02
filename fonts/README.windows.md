# Windows 中文字体

本目录保留 Wine 原有字体，并加入以下 Windows 中文字体原文件：

| 文件 | 字体 | 版本 |
| --- | --- | --- |
| `simsun.ttc` | 宋体 / 新宋体 | 5.15 |
| `simhei.ttf` | 黑体 | 5.03 |
| `msyh.ttc` | 微软雅黑 / Microsoft YaHei UI | 6.23 |
| `msyhbd.ttc` | 微软雅黑 / Microsoft YaHei UI 粗体 | 6.23 |
| `msyhl.ttc` | Microsoft YaHei Light / UI Light | 6.21 |
| `mingliu.ttc` | 細明體 / 新細明體 / MingLiU_HKSCS | 7.01 |

字体于 2026-09-26 从 Microsoft 字体服务下载，原文件共 79,868,052 字节。
来源目录为 <https://fs.microsoft.com/fs/windows/fontset-2017-04.json>；
2026-10-03 另加入用户本地《霹雳奇侠传》资料中的 `mingliu.ttc` 原文件，27,506,260 字节；内部字族名称已核对。每个文件的来源、大小和 SHA-256 见 `windows-fonts.json`。
字体版权及文件内嵌信息归原权利人所有，不适用 Wine 的 LGPL 许可证。

主工程 CI 的 `all` 模式经 `package-amd64.py` 将本目录的 `.ttf` 和 `.ttc`
字体与原有字体一同复制到 `switch/wine/share/wine/fonts/` 和
`switch/wine/drive_c/windows/fonts/`，再合并进最终 `autorun.zip`。
本说明及来源清单随包保存在 `share/wine/fonts/`。
Wine 在启动时扫描 `C:\windows\fonts`；无需为这些字体追加游戏适配包或注册表配置。
