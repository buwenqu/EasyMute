# EasyMute

> 常驻系统托盘、一键静音「当前焦点应用」的极致轻量 Windows 小工具。

在任意窗口按下快捷键（默认 `Ctrl + Alt + M`），即可静音 / 恢复**当前正在发声的应用**——等价于音量合成器中该应用的静音开关，但只需要一次按键。

- 📄 [需求文档（PRD）](docs/需求文档.md)
- 📄 [可行性验证报告](docs/可行性验证报告.md)

## ✨ 功能特性

| 功能             | 说明                                                                                                               |
| ---------------- | ------------------------------------------------------------------------------------------------------------------ |
| 全局快捷键       | 组合键（Ctrl / Alt / Shift / Win）或单键（F1–F24 / Pause / ScrollLock），完全可自定义，默认 `Ctrl + Alt + M`       |
| 一键静音焦点应用 | 自动识别前台窗口所属进程：支持多进程应用（浏览器等）、子进程（Electron / 小程序宿主）、UWP（ApplicationFrameHost） |
| 托盘常驻         | 无主窗口、无控制台黑框；右键菜单：设置 / 退出；双击图标打开设置                                                    |
| 设置窗口         | 自定义快捷键（点击录入框直接按键）、开机自启、气泡提示开关；改动即时生效                                           |
| 开机自启         | 写入 `HKCU\...\Run`，免管理员权限；程序移动路径后自动修正                                                          |
| 单实例           | 重复启动只唤起已有实例的设置窗口                                                                                   |

### 设计指标

| 指标       | 目标   | 实测（x64 Release，MinGW 静态构建） |
| ---------- | ------ | ----------------------------------- |
| 单文件体积 | ≤ 2 MB | ≈ 0.8 MB                            |
| 空闲 CPU   | ≈ 0%   | ≈ 0%（纯事件驱动，无轮询）          |
| 运行时依赖 | 无     | 仅系统 DLL                          |

## 🚀 快速开始

1. 获取 `EasyMute.exe`（[Releases 下载](https://github.com/buwenqu/EasyMute/releases/latest) 或自行构建）；
2. 双击运行——程序静默驻留系统托盘（默认在右下角折叠区）；
3. 在任意应用窗口按下 `Ctrl + Alt + M` 即可静音 / 恢复该应用的音量。

| 操作             | 说明                                      |
| ---------------- | ----------------------------------------- |
| `Ctrl + Alt + M` | 静音 / 恢复当前焦点应用（可在设置中修改） |
| 右键托盘图标     | 菜单：**设置…** / **退出**                |
| 双击托盘图标     | 打开设置窗口                              |

> 提示：程序**不需要管理员权限**。若托盘图标被折叠，可在「系统设置 → 个性化 → 任务栏 → 其他系统托盘图标」中将其设为始终显示。

## 🛠 构建

### 环境要求

- Windows 10 1809+ / Windows 11（x64）
- CMake ≥ 3.20
- 以下编译器之一：
  - **MinGW-w64（推荐，本仓库已实测）**：`winget install BrechtSanders.WinLibs.POSIX.UCRT`（包含 GCC、CMake、Ninja、windres）
  - **Visual Studio 2022 / Build Tools**（MSVC，CMake 自动识别）

### MinGW-w64（Ninja，已实测）

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
# 产物：build\EasyMute.exe
```

### Visual Studio 2022（MSVC）

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
# 产物：build\Release\EasyMute.exe
```

> `res\app.ico` 已生成并入库；如需重新生成图标，运行 `.\tools\make_icon.ps1`。

### VS Code 编辑器提示（IntelliSense）

若编辑器把 `QueryFullProcessImageNameW`、`CompareStringOrdinal` 等 **Vista+ API 标为“未定义标识符”**：这是 IntelliSense 所用工具链的 `WINVER` 偏低所致（这些声明受头文件 `#if WINVER >= 0x0600` 保护）；**不影响实际构建**（CMake 已显式定义 `WINVER/_WIN32_WINNT=0x0601`）。

若希望编辑器同样零告警，可**在本地**创建 `.vscode/c_cpp_properties.json`（含机器路径，已加入 .gitignore，请勿提交），模板如下（把 `compilerPath` 换成本机 MinGW 的 `g++.exe`；MSVC 用户可改用 `windows-msvc-x64` 模式并省略该字段）：

```jsonc
{
  "version": 4,
  "configurations": [
    {
      "name": "Win32-MinGW",
      "compilerPath": "C:/path/to/mingw64/bin/g++.exe",
      "intelliSenseMode": "windows-gcc-x64",
      "cStandard": "c17",
      "cppStandard": "c++17",
      "defines": [
        "UNICODE",
        "_UNICODE",
        "WIN32_LEAN_AND_MEAN",
        "NOMINMAX",
        "WINVER=0x0601",
        "_WIN32_WINNT=0x0601",
      ],
      "includePath": ["${workspaceFolder}/src", "${workspaceFolder}/res"],
    },
  ],
}
```

### 验证工具（可选）

仓库附带两个控制台小工具，用于验证「进程匹配 → 会话静音」核心链路：

```powershell
# 1) 启动一个“渲染静音”的测试进程，制造一个真实音频会话
.\build\easymute_soundstub.exe

# 2) 列出默认输出设备上的全部音频会话
.\build\easymute_selftest.exe list

# 3) 对指定进程执行「静音 → 恢复」两次切换（净效果为还原原状态）
.\build\easymute_selftest.exe <pid>
```

实测输出样例（对测试桩进程）：

```text
[1] 目标应用: easymute_soundstub.exe (pid=16788)
    进程路径: D:\...\build\easymute_soundstub.exe
    关联进程数: 7
[2] 匹配音频会话: 1 个（其中已静音 0 个）
[3] 第一次切换后: 已静音
[4] 二次切换（还原）后: 已恢复
```

## ⚙️ 配置文件

配置保存在 `%APPDATA%\EasyMute\config.ini`（界面修改即时写入）：

```ini
[Hotkey]
Modifiers=3   ; 1=Alt 2=Ctrl 4=Shift 8=Win，可按位相加
Key=77        ; 虚拟键码（77 = M）
[General]
AutoStart=0
ShowNotification=1
```

## 🧱 项目结构

```text
EasyMute/
├─ CMakeLists.txt
├─ src/
│  ├─ main.cpp            # 入口：COM 初始化、单实例、消息循环
│  ├─ App.*               # 应用控制器：托盘菜单、热键分发、设置联动
│  ├─ AppResolver.*       # 前台窗口 → 进程 + 父链 + 子进程集合
│  ├─ AudioSession.*      # WASAPI 音频会话枚举与静音切换（核心）
│  ├─ HotkeyManager.*     # 全局热键注册（先验证新键，失败保留旧键）
│  ├─ HotkeyName.*        # 快捷键格式化与合法性校验
│  ├─ HotkeyBox.*         # 快捷键录入控件（自绘）
│  ├─ SettingsDialog.*    # 设置窗口
│  ├─ TrayIcon.*          # 托盘图标与气泡提示
│  ├─ Config.*            # INI 配置读写
│  ├─ AutoStart.*         # 开机自启（HKCU Run）
│  └─ ComPtr.h / resource.h
├─ res/                   # 应用图标、资源脚本（.rc）、应用清单（manifest）
├─ tools/                 # 自检工具、测试桩、图标生成脚本
└─ docs/                  # 需求文档与可行性验证报告
```

## 🔍 技术要点

- **按应用静音**：`IMMDeviceEnumerator → IAudioSessionManager2 → IAudioSessionControl2 / ISimpleAudioVolume`；会话按「PID ∈ 进程树 ∪ 同可执行文件路径」匹配，覆盖浏览器多进程、Electron 子进程、UWP 宿主等场景；多会话统一取反，保证“要么全静、要么全响”；
- **全局热键**：`RegisterHotKey` + `MOD_NOREPEAT`（防长按连发）；设置时直接接受任意合法组合（不拦截、不检测"被占用"），注册完全静默：失败时保留旧键可用，界面不提示；
- **托盘图标**：`Shell_NotifyIcon`；处理 `TaskbarCreated`，资源管理器重启后自动恢复；
- **零打扰**：无后台轮询、无常驻线程、不注入、不挂钩子、不联网。

> 详细技术验证与风险分析见 [可行性验证报告](docs/可行性验证报告.md)。

## ⚠️ 已知限制

- 极少数使用「独占模式」直写硬件的应用不受会话音量控制；
- 个别全屏独占游戏 / 高权限窗口焦点下可能收不到全局热键（欢迎反馈具体场景，后续可提供兜底方案）；
- 单键模式仅限低冲突键（F1–F24 / Pause / ScrollLock）；字母、数字等高频键必须配合修饰键，避免全系统按键被抢占。

## � 排查

- 设环境变量 `EASYMUTE_DEBUG=1` 后运行，调试日志写入 `%TEMP%\easymute_debug.log`；
- `tools/` 下的 `easymute_selftest.exe`（`list` / `<pid>`）与 `easymute_soundstub.exe` 可用于验证核心链路；
- 快捷键无效：组合可能被其他软件占用——程序不显示占用状态，在设置窗口换一个组合重新录入即可；录入时直接按“当前正在使用的快捷键”不会没反应，会正常回显并结束采集。

## �🗺 Roadmap

- [x] M1 核心链路验证（焦点进程 → 会话静音）
- [x] M2 MVP（托盘 + 热键 + 设置 + 自启 + 单实例）
- [ ] M3 打磨：热键冲突提示细化、多机实测
- [ ] P1：静音状态记忆、界面多语言
- [ ] P2：音量调节、多快捷键、按住静音

## 许可证

暂未指定（如需开源请补充 LICENSE 文件）。
