# Xiangqi Desktop

一个用 C++20、CMake 和 Win32/GDI 写的中国象棋桌面程序。

## 功能

- 初始化标准中国象棋棋盘。
- 支持鼠标点击选中棋子、点击目标位置走棋。
- 你执红方，电脑执黑方。
- 校验车、马、相/象、仕/士、帅/将、炮、兵/卒的基础走法。
- 支持吃子、炮隔山打、马腿、象眼、九宫、过河规则。
- 吃掉对方帅/将后结束游戏。
- 右上角可以重新开始。

## 构建

```powershell
cmake -S . -B build
cmake --build build
```

## Release 打包

生成可拷贝到其他 Windows 电脑直接运行的发布目录和 zip：

```powershell
.\package_release.ps1
```

输出位置：

```text
dist\pikafish-xiangqi-gui
dist\pikafish-xiangqi-gui-windows-x64.zip
```

发布包会包含：

- `xiangqi.exe`
- 已安装外部引擎的 Windows 可执行文件
- Pikafish 所需的 `pikafish.nnue`
- Fairy-Stockfish 所需的 `variants.ini`
- README 和诊断脚本

可以在发布目录里运行：

```powershell
.\check_engines.ps1
```

如果已安装的外部引擎都输出 `bestmove`，说明这台电脑可以直接运行。
想查看支持但未安装的引擎，可以运行：

```powershell
.\check_engines.ps1 -ShowMissing
```

## 运行

```powershell
.\build\bin\Debug\xiangqi.exe
```

如果使用单配置生成器，例如 Ninja，运行路径通常是：

```powershell
.\build\bin\xiangqi.exe
```

## 玩法

- 左键点击红方棋子进行选中。
- 再点击目标交叉点完成移动。
- 如果点到另一个红方棋子，会切换选中。
- 电脑会在你走棋后自动走一步。
- 点击 `提示` 会用当前选择的引擎给红方生成建议，并用蓝色框标出起点和终点。
- 点击 `认输` 会立即结束本局并判电脑获胜。

## 强引擎

程序支持在界面右上角选择电脑引擎。每一步电脑思考前都会读取当前下拉框选择，因此一局中途也可以切换引擎，下一步立即生效。

程序启动时只会在下拉框里显示实际可用的引擎。当前支持这些引擎类型：

- 本机内置
- Pikafish
- Fairy-Stockfish
- ElephantEye
- ElephantArt
- PX0

安装方式：

```powershell
.\install_pikafish.ps1
```

如果 GitHub 下载临时失败，也可以手动下载对应发布包并解压到：

```text
pikafish-xiangqi-gui\engines\pikafish
pikafish-xiangqi-gui\engines\fairy-stockfish
pikafish-xiangqi-gui\engines\eleeye
pikafish-xiangqi-gui\engines\elephantart
pikafish-xiangqi-gui\engines\px0
```

程序会自动查找这些目录或其 `Windows` 子目录里的 `.exe`。

`本机内置` 不需要外部文件。其他引擎如果没有安装，不会显示在下拉框里；如果运行过程中外部引擎不可用，会退回内置 alpha-beta 搜索。

当前外部引擎每步思考时间默认是 `500ms`，并在后台线程运行，所以电脑思考时窗口不会卡住。可以在 `src/main.cpp` 里修改 `kEngineThinkTimeMs` 调整棋力和等待时间。

## 裁判逻辑

- 不能走会导致己方将/帅被攻击的棋。
- 吃掉对方将/帅立即获胜。
- 即使没有吃将，只要对手已经没有合法走法，也会判定当前走棋方获胜。

## 后续增强方向

如果想继续增强，有两条路线：

1. 继续完善内置引擎：加入“不能送将”“将军检测”“长将/长捉规则”、更细的位置表、迭代加深、置换表、静态搜索。
2. 接入成熟引擎：例如 Pikafish、Fairy-Stockfish 或 ElephantEye，通过 UCI/UCCI 协议让本程序只负责界面和走法显示。

成熟项目参考：

- Pikafish: https://github.com/official-pikafish/Pikafish
- Fairy-Stockfish: https://github.com/fairy-stockfish/Fairy-Stockfish
- ElephantEye/xqbase: https://github.com/xqbase
