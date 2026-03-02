# windows-TO-TV

Windows 全屏看电视（Chrome Kiosk）控制程序，纯 Win32 C++ 实现，`wWinMain` 入口（`/SUBSYSTEM:WINDOWS`），运行时不会弹出控制台黑框。

## 功能

- 启动时先结束所有 `chrome.exe`，等待退出后再按配置命令启动。 
- 从 `config.txt` 读取 5 个配置项：
  - `chromePath`
  - `userDataDir`
  - `channels`（`|` 分隔）
  - `shutdownKey`
  - `lastpage`（程序换台成功后自动更新）
- 全局热键监听：`↑ ↓ ← →` + `shutdownKey`
  - `↑ / ↓`：系统主音量每次 ±7%
  - `← / →`：按频道列表循环换台（会先重启 Chrome）
  - `shutdownKey`：立即关机（`shutdown /s /t 0 /f`）
- 稳定性机制：
  - 杀进程后等待 `chrome.exe` 真正退出
  - 启动后检测 Chrome 主窗口是否出现
  - 启动失败自动重试
  - 换台失败时自动回退到当前频道
  - 换台后将 Chrome 窗口置顶并激活

## 文件

- `tv_kiosk.cpp`：主程序。
- `config.txt`：示例配置（UTF-8）。

## Visual Studio 编译

### 方式一：命令行（Developer Command Prompt for VS）

```bat
cl /std:c++20 /EHsc /W4 /DUNICODE /D_UNICODE tv_kiosk.cpp /link /SUBSYSTEM:WINDOWS Ole32.lib User32.lib
```

生成 `tv_kiosk.exe` 后直接双击运行。

### 方式二：Visual Studio IDE

1. 新建“空项目”（C++）。
2. 把 `tv_kiosk.cpp` 和 `config.txt` 加入项目目录。
3. 项目属性设置：
   - C/C++ -> Language -> C++ Language Standard: `ISO C++20`
   - Linker -> System -> SubSystem: `Windows (/SUBSYSTEM:WINDOWS)`
4. 链接库增加：`Ole32.lib`（`User32.lib` 默认通常已有）。

## `shutdownKey` 支持

- 字母：`A` ~ `Z`
- 主键盘数字：`0` ~ `9`
- 小键盘数字：`NUMPAD0` ~ `NUMPAD9`
- 功能键：`F1` ~ `F12`
- 常用键：`ESC`、`ENTER`、`SPACE`
- 方向键：`LEFT`、`RIGHT`、`UP`、`DOWN`
- 导航编辑键：`TAB`、`BACKSPACE`、`DELETE`、`HOME`、`END`、`PGUP`、`PGDN`、`INS`
- 或直接填虚拟键码数字（0~255）

