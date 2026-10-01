# Zero Escape: Zero Time Dilemma 适配包

适用于已核对的 Steam AppID 311240 游戏文件及 32 位 `Zero Escape.exe`。本包不包含游戏本体、存档、Steam API 或 DXVK DLL，也不修改游戏 EXE。需要支持适配包 API 19 的 AutoRun 主程序和配套运行时。

安装内容：

- 游戏目录下安装 OpenAL Soft 1.25.2 的 32 位 `OpenAL32.dll` 及许可证。它补齐游戏导入的 OpenAL 符号。
- 创建 `C:\users\Public\Documents\Steam\RUNE\311240`，供本测试版本的 Steam API 写入数据。适配包通过目录内的 `profile-directory.txt` 创建父目录；恢复配置只移除该占位文件，不删除游戏后来写入的数据。
- 设置游戏专属 `d3d=dxvk`、`controller=keyboard` 和已在标题菜单验证的按键：方向键/左摇杆移动、A 确认、B 取消。其他键位以 `Zero Escape.keys.txt` 为准。
- 设置 `register-com32=dinput8.dll`。新 Wine 前缀首次启动游戏时，主程序自动运行 32 位注册助手，在当前前缀注册运行时自带的 DirectInput8 DLL，成功后继续启动游戏。

## 首次使用

1. 安装支持 API 19 的主程序与运行时，然后在启动器中给 `Zero Escape.exe` 安装此适配包。
2. 启动游戏。首次启动会自动注册 DirectInput8；若注册失败，游戏不会继续启动，日志会出现 `[COM32]` 或 `[AUTORUN COM32]`。

当前设备已验证游戏进入标题菜单、A/B 和方向输入正常。原版 DXVK 3.1.1、1280×720 下默认频率约 15 FPS；超频后一次稳定段约 24 FPS。Sarek DX11 对照约 14 FPS，已从设备恢复原版 DXVK。游戏剧情、视频、存档和长时间运行尚未验收。

OpenAL Soft 二进制取自 [官方 1.25.2 Windows 发布包](https://github.com/kcat/openal-soft/releases/tag/1.25.2)，`bin/Win32/soft_oal.dll` 按发布包说明重命名为 `OpenAL32.dll`。源代码和许可证见该发布页及本目录的 `OpenAL32.LICENSE`、`OpenAL32-pffft.LICENSE`。
