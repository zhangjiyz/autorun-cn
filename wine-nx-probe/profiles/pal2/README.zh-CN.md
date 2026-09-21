# 仙剑奇侠传二 1.05 版配置

适用于根目录中的 `Pal2.exe`：

- PE 类型：32 位 i386 Windows GUI
- 文件大小：1268736 字节
- SHA-256：`f82059aee4870ec80c8664a12249e691540ca4861adfcc8d1c1f1ffd542846b1`
- 游戏说明文件标注版本：1.05

主程序使用 DirectDraw、DirectInput、DirectInput 8、Bink 和 Miles Sound System。
首版配置使用 Wine DirectDraw、4:3 等比居中、简体中文区域设置和独立键盘映射，
同时启用 SD 文件状态缓存及干净写句柄读取缓存。没有启用新仙剑 XP 版专用的
`wined3d-frontbuffer-swap`；只有实机出现视频后黑屏或主表面不提交时才应单独验证。

游戏本体必须完整复制。`Pal2.exe` 同目录至少要保留 `BINKW32.DLL`、
`MSS32.DLL`、`CHINA.DLL`、`Mp3dec.asi`、`Mss16.dll`、`keyboard.dat` 和
`Joystick.ini`，并保留 `Data`、`Fight`、`Game_Data`、`Music`、`PicLib`、
`Save`、`video` 等数据目录。2026-09-21 取得的旧真机日志缺少前三个必需 DLL，
加载器因此以 `c0000135` 退出，尚未形成显示、输入或音频兼容性结论。

## 控件

| Switch 控件 | 游戏输入 |
|---|---|
| 左摇杆、十字键 | 方向键 |
| A / B | Enter / Esc |
| X / Y | 空格 / Ctrl |
| L / R | Page Up / Page Down |
| + / - | Enter / Esc |
| 右摇杆、触屏 | 鼠标 |
| ZR / ZL | 鼠标左键 / 右键 |
| 左摇杆按下 | Shift |

## 首轮实机验收

1. 从根目录启动 `Pal2.exe`。
2. 确认寰宇之星、大宇或狂徒标志及开场 Bink 视频能显示并有声音。
3. 确认主菜单画面为 4:3 居中，十字键、左摇杆、A、B 和触屏可用。
4. 进入游戏检查背景音乐、语音、音效、地图和战斗画面。
5. 新建存档，完全退出后重新进入并读档。
6. 如果视频后黑屏、只有声音或首次菜单长时间停顿，保留本轮 `game-Pal2.log`。
