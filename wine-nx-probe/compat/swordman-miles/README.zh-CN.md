# Swordman Miles 兼容层

适用原版 `mss32.dll`：331776 字节，SHA-256
`6a128953250b3d142245a9ca6facabf308666703c1f9c8c5b6ac95acce95403d`。

此版本的音频定时器 `mss32.dll+0x13b0` 在配置项 18 非零时，先对游戏
主线程调用 `SuspendThread`。返回 `-1` 就跳过全部音频回调。
Horizon 当前拒绝暂停已运行线程，因此 DirectSound 可以正常播放并移动
游标，Miles 的缓冲区仍然持续为空。首次真机测试确认音乐恢复、视频
不再停在单帧。随后将代码校验限定到主程序，真机已确认音乐与视频
画面、声音能够连续播放。

兼容层转发原版全部 315 个导出，保留名称、序号和调用约定。仅在
`AIL_startup` 后用原库的 `AIL_set_preference(18, 0)` 关闭这一保护，并
拦截后续对配置项 18 的写入。其余配置项直接交给原库。
该绕过减少了 Miles 原有的主线程同步保护，必须检查音乐、音效、视频
和退出的持续运行表现；不会给其他游戏打开此行为。

构建（工具链须支持 Windows i386 PE）：

```sh
python3 wine-nx-probe/compat/swordman-miles/build.py /path/to/original/mss32.dll /path/to/output --cc /path/to/i686-w64-mingw32-clang
```

构建会拒绝其他 DLL 版本，并校验导出表中 2 个本地入口、313 个原版
转发入口。无需重新编译 NRO 或系统 DLL。

定向验证（需要 Unicorn，仅读取用户提供的原库）：

```sh
python3 wine-nx-probe/compat/swordman-miles/verify_timer.py /path/to/original/mss32.dll
python3 wine-nx-probe/compat/swordman-miles/verify_proxy.py /path/to/output/mss32.dll
```

部署时先校验设备原库，再将其改名为 `mss32_autorun_original.dll`，
将构建的兼容层放到同一游戏目录的 `mss32.dll`。兼容层不写调试日志。
必须完全退出再启动游戏。回滚只需移走兼容层，把原库改回
`mss32.dll`；游戏 EXE、数据与存档均无需修改。

已验证的游戏配置保留在 `settings/`。将三个 `Swordman.*.txt` 文件放到
游戏目录；`ddraw-overrides.ini` 中的键仅合并到原 `ddraw.ini` 的
`[ddraw]` 节，保留原文件其余设置。游戏原生模式为 320×200×16，
输出为 1280×720，保持 16:10 比例并居中。

`Swordman.box64.txt` 使用动态编译、关闭 CALLRET、将自修改代码校验
限定到主程序（SELFMOD=2）。`Swordman.wine-nx.txt` 显式关闭采样分析。
键位包括方向键、Enter、Esc、Space、Tab、PageUp/Down、F1/F2 和触屏鼠标。

运行时还需本次保留的两个修正：允许 DOS/PE 头重叠的打包 EXE，及
对无效非空 HWND 的 MessageBox 返回 0/ERROR_INVALID_WINDOW_HANDLE。
配置针对本次 `Swordman.exe`，SHA-256：
`b1d7dba03506427fc3f9046f16ea564f454f5949a21cfc701b1861369fc77ae2`。
