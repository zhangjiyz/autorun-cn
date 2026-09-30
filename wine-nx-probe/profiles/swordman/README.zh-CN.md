# 天地劫：神魔至尊传 Swordman V1.3 适配包

本包针对 SHA-256 为 `b1d7dba03506427fc3f9046f16ea564f454f5949a21cfc701b1861369fc77ae2` 的 `Swordman.exe`，要求运行时适配包 API 17。它安装运行设置、按键、Box64 参数、封面，并整文件替换游戏目录中的 `ddraw.dll`、`ddraw.ini` 和 Miles 代理 `mss32.dll`。适配包会备份被替换文件，支持“恢复上次配置”。

`ddraw.dll` 和完整的 `ddraw.ini` 来自已适配的 Swordman 游戏目录；二者 SHA-256 分别为 `85e0f7d530dfda134793a57cb3e76b0287dcc96892ee57162dd68f47283b03a9` 和 `3a8def500843d57da27b47b3a129018e6f9b93cbf76ecb0d0655375706fb64fa`。`ddraw.ini` 是整文件替换，安装后原有自定义设置会进入适配包备份。cnc-ddraw 的 MIT 许可随包安装为 `cnc-ddraw.LICENSE`。

Miles 代理的 SHA-256 为 `24f1b4ad5892409720b3ecf373b533e1feba45277b272fa73d2a5f50a3307d5d`，其 LGPL 2.1 许可随包安装为 `mss32-proxy.LICENSE`。它需要游戏目录中已有 `mss32_autorun_original.dll`，其内容必须是 SHA-256 为 `6a128953250b3d142245a9ca6facabf308666703c1f9c8c5b6ac95acce95403d` 的原版 `mss32.dll`。首次在未适配的游戏目录安装本包前，先将该原版文件复制为 `mss32_autorun_original.dll`；包中不含原版游戏 DLL。缺少此别名时，代理不能正常转发 Miles 调用。该操作和文件的撤销需要手动进行，适配包恢复只处理包内替换的目标文件。

安装后完全退出并重新启动游戏，分别核对画面位置、音乐、音效、视频、按键、存读档及退出。历史真机记录见 [`../../compat/swordman-miles/README.zh-CN.md`](../../compat/swordman-miles/README.zh-CN.md)；本次打包不等于新的真机验证。
