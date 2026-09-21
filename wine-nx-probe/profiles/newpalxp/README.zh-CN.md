# 新仙剑奇侠传 2001 `Palgame.exe` 实机配置

仅适用于以下主程序：

- 文件名：`Palgame.exe`
- PE 类型：32 位 i386 Windows GUI
- 时间戳：2001-08-03 09:15:47
- SHA-256：`6c78208f89dc83a13a49512d783e3b6c02ad6768cfa82f67d914c1b8afa7794c`

这不是 `NewPAL_Release.exe` 2.17.102.0。该版本直接导入 DirectDraw、
DirectInput、Miles Sound System 和 Bink，因此不继承旧适配中的 DXVK 和
SDL DirectSound 设置。

启动和视频结束后，程序会用读写权限反复打开 `gfight1.dat`、`ghero.dat`、
`gmap.dat` 等十个全局数据文件并查询文件信息。此适配启用
`sd-stat-cache=1`，避免相同句柄重复访问 SD 元数据。第一次打开游戏菜单时，
程序还会对 `gitem.dat` 做上万次 1 至 4 字节随机读取，同时读取 `ghero.dat`
和 `gmagic.dat`。运行时允许尚未实际写入的读写句柄使用 SD 字节缓存；第一次
真实写入或截断前会使同路径的全部缓存失效，重命名和删除也会失效缓存，因而
不改变存档写入语义。修复前菜单阶段约产生 3.5 万次真实 SD 请求，修复后实机
确认首次菜单速度恢复正常。该行为由本配置的 `sd-clean-writer-cache=1` 单独启用，
未启用的其他游戏继续使用原来的只读字节缓存规则。

当前配置启用简体中文区域设置、4:3 窗口等比居中、强制键盘和独立按键。
实机日志证明 Bink 视频结束后，游戏主线程、Miles 音频和输入仍在运行。正式
游戏阶段用 `Blt` 把 640×480 离屏表面复制到 DirectDraw 主表面，但 OpenGL
接管屏幕后，主表面的 HWND clipper 在 Wine-NX 上返回空可见区；Wine 因而返回
`DD_OK`，却没有复制或提交任何画面。纯
`wined3d-renderer=gdi` 实机 A/B 虽能显示游戏画面，却使开场视频无声且游戏
帧率不可接受，已撤回。进一步的实机日志表明，视频阶段通过 Present 正常提交
OpenGL 帧，视频后游戏却改为直接更新 DirectDraw 主表面；桌面 WGL 可直接显示
GL_FRONT 更新，但 Wine-NX 的双缓冲 EGL 屏幕不会显示这条路径。当前配置仅在
该游戏启用显式前缓冲提交：主表面 `Blt` 绕过无效的桌面可见区裁剪，把结果画到
GL_BACK 并执行交换，继续保留 GPU 路径。实机已确认直接载入存档后的游戏画面、
输入、背景音乐、按键音效和菜单均可用。

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

## 实机验收

1. 从根目录启动 `Palgame.exe`，不要启动 `Data/Palgame.exe`。
2. 确认启动弹窗的五个中文按钮不再显示问号。
3. 记录是否出现开场视频、主菜单以及 4:3 居中画面；先让开场视频自然播放
   完毕，再单独测试 B / Esc 跳过路径。
4. 在主菜单验证十字键、左摇杆、A、B 和触屏落点。
5. 进入游戏验证背景音乐、音效和一段 Bink 视频。
6. 新建存档，退出 Autorun 后重新进入并读档。
7. 若失败，保留游戏日志，并记录是否生成或更新 `ddraw.log`。

当前实机已验证直接载入存档后的显示、输入、背景音乐、按键音效和首次菜单
速度。正式发布前仍应完整验证开场视频自然结束与跳过、游戏内视频、存档写入、
退出重启和持续运行。
