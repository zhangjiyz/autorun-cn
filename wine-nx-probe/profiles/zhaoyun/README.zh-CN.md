# 《三国赵云传》手柄配置

将 `Game.keys.txt` 放在 Switch 的 `switch/wine/drive_c/ZhaoYun/Game.exe` 旁边，并在 Autorun 的游戏设置中启用“使用游戏专用按键”。此文件只影响这款游戏。

这份 2002ls 版本的游戏会用 DirectInput 相对位移更新自己的光标。要让触屏点哪里就定位到哪里，先对你自己的 `Game.exe` 运行 `python3 patch-game.py /路径/Game.exe`，再复制游戏到 SD 卡。脚本只接受已核对哈希的 2002ls 可执行文件，修改前会保存 `Game.exe.autorun-before-absolute-mouse`；`--restore` 可恢复原文件。安装包不含游戏本体。

在 `Game.exe` 旁边的 `Game.wine-nx.txt` 中设置：

```text
own-controls=1
aspect-fit=800x600
touch-coordinates=screen
left-stick-run=shift
left-stick-eight-way=1
left-stick-aim=140
left-stick-move=mouse
```

`aspect-fit` 将完整的 4:3 游戏画面居中；`touch-coordinates=screen` 让触屏坐标按 Switch 的 1280×720 桌面传给这款游戏。只有完成上述可执行文件补丁后，游戏自己的光标才会读取该位置。游戏的键盘方向键在实机上只让角色朝四个方向移动，因此 `left-stick-move=mouse` 让默认摇杆移动改用游戏自己的鼠标路径：光标指向所推方向并按住左键。`left-stick-eight-way=1` 将摇杆分成八个方向；`left-stick-aim=140` 把光标放在画面中心附近前方 140 个 Switch 屏幕像素处，距离仍需实机确认。按住 L3 时恢复键盘方向键步行，目前仍只有四向。默认摇杆的走／跑速度由游戏底部的“走／跑”按钮决定；若设为“跑”，鼠标移动路径会跑。触屏按住时仍由触点定位。触点定位、菜单点击和摇杆鼠标斜向移动已在 Switch 上确认；攻击与菜单的相互作用仍待上机验证。

| Switch 操作 | 游戏操作 |
|---|---|
| 左摇杆 | 八方向定位光标并按住鼠标左键移动；按住 L3 改用四方向键步行。游戏底部切到“跑”后，默认鼠标移动会跑 |
| 右摇杆左 / 上 / 右 | 键盘 A / S / D，切换弓 / 剑 / 枪；右摇杆下不映射 |
| 十字键上 / 下 / 左 / 右 | 数字键 1 / 2 / 3 / 4，使用道具快捷栏 |
| L / ZL / R / ZR | 数字键 5 / 6 / 7 / 8，使用道具快捷栏 |
| A | 空格，对话或普通攻击 |
| B | 鼠标右键，必杀技及游戏中的右键操作 |
| X | Ctrl；可与触屏左键一起使用。Ctrl＋空格能否代替 Ctrl＋左键尚未验证 |
| Y | C，属性界面 |
| − / + | Esc，物品栏或结束对话 / Tab，小地图 |
| 触屏 | 直接定位鼠标并按左键；物品栏中轻点拿起、再点目标位置放下，也用于菜单和技能界面 |

右摇杆按下不映射。物品栏需要第二次点击才放下，这是游戏原本的操作方式。手柄的键盘和鼠标操作仍需在 Switch 上逐项实测，特别是武器切换、快捷栏、必杀技及 Ctrl 组合操作。
