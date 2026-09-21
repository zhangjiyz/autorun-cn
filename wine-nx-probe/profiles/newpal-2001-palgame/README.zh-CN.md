# 新仙剑奇侠传 2001 `Palgame.exe`：首轮实机配置

仅适用于以下主程序：

- 文件名：`Palgame.exe`
- PE 类型：32 位 i386 Windows GUI
- 时间戳：2001-08-03 09:15:47
- SHA-256：`6c78208f89dc83a13a49512d783e3b6c02ad6768cfa82f67d914c1b8afa7794c`

这不是 `NewPAL_Release.exe` 2.17.102.0。该版本直接导入 DirectDraw、
DirectInput、Miles Sound System 和 Bink，因此不继承旧适配中的 DXVK、
SDL DirectSound 和 `sd-stat-cache` 设置。

当前配置启用简体中文区域设置、4:3 窗口等比居中、强制键盘和独立按键。
实机日志证明 Bink 视频结束（自然结束和 B / Esc 跳过均相同）后，游戏主线程、
Miles 音频和输入仍在运行，但 OpenGL 呈现计数永久停止；WineD3D 创建的屏幕
EGL surface 也没有销毁，因而合成器无法接回游戏的 2D 窗口画面。纯
`wined3d-renderer=gdi` 实机 A/B 虽能显示游戏画面，却使开场视频无声且游戏
帧率不可接受，已撤回。进一步的实机日志表明，视频阶段通过 Present 正常提交
OpenGL 帧，视频后游戏却改为直接更新 DirectDraw 主表面；桌面 WGL 可直接显示
GL_FRONT 更新，但 Wine-NX 的双缓冲 EGL 屏幕不会显示这条路径。当前配置仅为
该游戏把主表面更新画到 GL_BACK 并执行交换，继续保留 GPU 路径。这一修复仍需
分别验证自然播放结束和 B / Esc 跳过路径。

游戏目录自带的 `DDraw.dll` 是 DDrawCompat；Windows 侧 `ddraw.log` 只能证明
它曾在原环境成功加载，不能视为 Wine-NX 兼容性证据。Switch 实机上该 DLL
虽能完成 hooks 安装，但随后使 `wined3d` 无法取得 GL context，并从
`ddraw.dll` 的空函数指针崩溃。因此设备目录中必须停用这份本地 `DDraw.dll`，
使用 Wine 内置 DirectDraw。

目录里的 `Data/Palgame.exe` 是另一份 2004 年、SHA-256 为
`1536f7f04045214c3cc42bf360e6abc80479eb3b3c36509da350cbbee7684141` 的程序，
不属于本适配目标，不应从 `Data` 子目录启动。

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

1. 从根目录启动 `Palgame.exe`，不要启动 `Data/Palgame.exe`。
2. 确认启动弹窗的五个中文按钮不再显示问号。
3. 记录是否出现开场视频、主菜单以及 4:3 居中画面；先让开场视频自然播放
   完毕，再单独测试 B / Esc 跳过路径。
4. 在主菜单验证十字键、左摇杆、A、B 和触屏落点。
5. 进入游戏验证背景音乐、音效和一段 Bink 视频。
6. 新建存档，退出 Autorun 后重新进入并读档。
7. 若失败，保留游戏日志，并记录是否生成或更新 `ddraw.log`。

本配置只通过静态文件和打包检查；显示、输入、音频、视频、存档和退出重启
均需 Switch 实机验证。
