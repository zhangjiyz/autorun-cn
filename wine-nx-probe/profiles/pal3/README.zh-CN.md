# 仙剑奇侠传三配置

首版配置针对根目录中的 `PAL3.exe`：

- PE 类型：32 位 i386 Windows GUI
- 文件大小：1878528 字节
- SHA-256：`aa11c350e7d592df47d527e0db2c09446f3051a0b6dd1614694e74f5278c27d5`
- 随游戏提供的 PAL3patch：2.1，构建于 2020-06-01

主程序使用 DirectInput 8、Bink 和 Miles Sound System，`gbengine.dll` 使用 Direct3D 9。
目录中的 PAL3patch 已启用免光盘、注册表重定向、语言检测、禁用键盘钩子和外部解包器。
首版配置使用 WineD3D、简体中文区域设置、游戏专用键鼠映射及两项 SD 缓存。当前运行环境中的 32 位 DXVK 会在 `d3d9.dll` 初始化阶段退出，因此本配置使用 WineD3D 进入游戏。

当前诊断版本还设置了 `wined3d-explicit-buffer-flush=0`。实机日志显示游戏进入黑屏后仍持续约 60 FPS 更新 shader 常量和交换画面，最终 D3D9 后台缓冲除鼠标外全黑；同时 Switch 专用的非一致持久映射路径每 10 秒执行约一万次显式缓冲刷新。因此先只对本游戏关闭该路径做单变量验证，其他游戏仍保持默认开启。此版本已上传实机，但标题画面结果尚未验收。

在 Switch 上使用 PAL3patch 2.1 时，`PAL3patch.conf` 建议使用以下兼容设置：

```ini
game_zbufferbits=max
game_multisample=0,0
reduceinputlatency=0
```

多重采样会增加 Bink 视频播放开销，并可能令游戏的渲染到纹理路径输出黑屏。PAL3patch 2.1 的深度缓存选项不支持后续版本使用的 `auto`，因此保留 `max`；关闭额外低延迟模式更适合当前 WineD3D 后端。

## 控件

| Switch 控件 | 游戏输入 |
|---|---|
| 左摇杆、十字键 | 方向键移动 |
| 右摇杆左右 | A / D 转动镜头 |
| A | 空格，互动 |
| B / + | Esc，菜单或取消 |
| X | Enter，确认 |
| Y / 右摇杆按下 | Tab，切换角色 |
| L / R | Q / E |
| - | M，大地图 |
| 左摇杆按下 | Shift |
| 触屏 | 鼠标移动和左键点击 |
| ZR / ZL | 鼠标左键 / 右键 |

## 首轮实机验收

1. 从游戏根目录启动 `PAL3.exe`，不要启动 `PatchConfig.exe` 或 `config.exe`。
2. 确认 PAL3patch 没有弹出错误，开场 Bink 视频能够显示并播放声音。
3. 确认主菜单、3D 场景、人物与界面正常显示，画面比例没有拉伸。
4. 检查左摇杆移动、右摇杆转镜头、A 互动、B 菜单以及触屏点击。
5. 检查背景音乐、语音、战斗音效、存档和读档。
6. 若启动缓慢、黑屏或帧率低，保留本轮 `game-PAL3.log`、`PAL3patch.log.txt` 和
   `PAL3patch.error.txt`，再单独测试抗锯齿、输入延迟或 WineD3D 回退。
