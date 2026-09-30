# 天地劫：神魔至尊传 Swordman V1.3 适配包

本包针对 SHA-256 为 `b1d7dba03506427fc3f9046f16ea564f454f5949a21cfc701b1861369fc77ae2` 的 `Swordman.exe`，要求运行时适配包 API 17。它安装运行设置、按键、Box64 参数、封面，并整文件替换游戏目录中的 `ddraw.dll`、`ddraw.ini` 和完整的 Miles 兼容库 `mss32.dll`。文件不存在时直接拷贝；文件存在时由适配包事务备份后覆盖，支持“恢复上次配置”。无需提前改名、准备原库别名或手动复制 DLL。

`ddraw.dll` 和完整的 `ddraw.ini` 来自已适配的 Swordman 游戏目录；二者 SHA-256 分别为 `85e0f7d530dfda134793a57cb3e76b0287dcc96892ee57162dd68f47283b03a9` 和 `3a8def500843d57da27b47b3a129018e6f9b93cbf76ecb0d0655375706fb64fa`。`ddraw.ini` 是整文件替换，安装后原有自定义设置会进入适配包备份。cnc-ddraw 的 MIT 许可随包安装为 `cnc-ddraw.LICENSE`。

完整 Miles 兼容库为 331776 字节，SHA-256 `f2d040218d7e63f83c799b005f688c1b4b150994442996b4c4929a46a1f15743`。构建阶段使用已核对的 Miles 5.0m 库，只将文件偏移 `0x13bd` 的 `TEST EAX,EAX` 改为 `XOR EAX,EAX`，使音频定时器的局部暂停标志为零；原有分支因此跳过 `SuspendThread` 与对应的 `ResumeThread`，不再因 Horizon 拒绝暂停线程而跳过音频服务。315 个原始导出、导入表、重定位与资源保持不变。修改说明及原版权归属随包安装为 `mss32-compat.NOTICE`。

包内 DLL 不含转发到 `mss32_autorun_original.dll` 的导出，也不加载该文件。安装时不寻找或校验用户提前准备的原库；已有手动别名保持原样。原库哈希只用于维护者构建此完整 DLL，不是用户安装前提。旧代理源码与复现工具仍保留在兼容目录中。

主机已验证文件缺失时安装、已有文件的备份覆盖与精确恢复、重复安装、旧别名不参与安装，以及每个提交中断/失败位置的回滚。指令仿真覆盖不同暂停配置、重定位后的装载地址、停止的定时器、回调参数和栈清理；完整 DLL 的新真机音频/视频验收仍待完成。

安装后完全退出并重新启动游戏，分别核对画面位置、音乐、音效、视频、按键、存读档及退出。历史真机记录见 [`../../compat/swordman-miles/README.zh-CN.md`](../../compat/swordman-miles/README.zh-CN.md)；本次打包不等于新的真机验证。
