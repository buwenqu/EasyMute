# AGENTS.md — EasyMute 项目开发记忆

> 本文件是仓库级开发记忆，随项目提交，克隆后即可被 AI 助手 / 新成员读取。
> 遇到问题先读本文件；新的经验教训请补充到末尾「经验更新日志」。

## 项目概览

- **EasyMute**：Windows 常驻托盘小工具——全局快捷键一键「静音 / 恢复当前焦点应用」的音频（等价于音量合成器中该应用的静音开关）。
- 体验目标：双击即用、免安装、无窗口静默驻留；空闲 CPU ≈ 0%（纯事件驱动，无轮询）；exe ≈ 0.80 MB（x64 Release）；零第三方运行时依赖。
- 范围外：音量数值调节、音频设备路由、跨平台、联网功能。
- 文档：`docs/需求文档.md`（PRD）、`docs/可行性验证报告.md`（含真机验证记录）、`README.md`（使用与构建说明）。
- 当前状态：v1.0.0 MVP 完成，核心链路已在真机端到端验证（详见可行性报告 §8）。

## 维护纪律

1. **验证后才提交**：改动后至少执行 `cmake --build build` + `easymute_selftest.exe list`（可先用 `easymute_soundstub.exe` 造一个真实音频会话）；涉及热键/设置改动时跑通交互验证。
2. **零依赖原则**：只用 Windows 系统 API（Win32/COM/WASAPI）+ C++ 标准库；禁止引入第三方库或运行时。
3. **安全底线**：不注入、不挂钩子（不使用 WH_KEYBOARD）、不联网、不申请管理员权限；保持"绿色单文件"。
4. **资源文件保持纯 ASCII**：`res/EasyMute.rc` 内不出现中文；界面文案一律在代码里用 `SetDlgItemTextW` 设置（原因见"已知坑"3）。
5. **源码 UTF-8**：所有 `.cpp/.h` 为 UTF-8（CMake 已为 MSVC 加 `/utf-8`）；宽字符串一律 `L"..."`。
6. **文档同步**：行为/需求变化必须同步更新 PRD、README、AGENTS.md（例如 2026-09-11 的"设置快捷键不再拦截被占用"需求变更）。
7. **提交信息**：中文、约定式前缀（`feat:` / `fix:` / `docs:` / `chore:`），一行标题 + 必要要点。

## 技术栈与命令

- 语言/标准：C++17（Win32 + WASAPI）
- 构建：CMake ≥ 3.20。已实测工具链 = Winget 安装的 **WinLibs MinGW-w64**（GCC 16 + Ninja + windres）：`winget install BrechtSanders.WinLibs.POSIX.UCRT`；MSVC 亦可（CMake 自动识别）。
- 常用命令（仓库根目录）：

  ```powershell
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # 首次配置
  cmake --build build                                        # 构建
  .\build\easymute_selftest.exe list                         # 列出现有音频会话
  .\build\easymute_selftest.exe <pid>                        # 对指定进程静音→恢复（净效果还原）
  .\build\easymute_selftest.exe config                       # 打印配置解析结果
  .\build\easymute_soundstub.exe                             # 制造一个真实音频会话（测试目标）
  .\tools\make_icon.ps1                                      # 重新生成 res\app.ico
  ```

- 调试日志：设 `EASYMUTE_DEBUG=1` 后运行主程序 → `%TEMP%\easymute_debug.log`。
- 产物：`build\EasyMute.exe`（GUI）、`build\easymute_selftest.exe`、`build\easymute_soundstub.exe`。

## 核心文件与职责

| 文件                                                               | 职责                                                               |
| ------------------------------------------------------------------ | ------------------------------------------------------------------ |
| `src/main.cpp`                                                     | 入口：`CoInitializeEx(MTA)`、单实例互斥、消息循环                  |
| `src/App.*`                                                        | 应用控制器：初始化、托盘菜单、`WM_HOTKEY` 分发、设置联动、提示     |
| `src/AppResolver.*`                                                | 前台窗口 → PID → 进程路径 + 父链 + 全子进程集合（含 UWP 宿主处理） |
| `src/AudioSession.*`                                               | WASAPI 会话枚举 / 匹配 / 静音切换；`selftest list` 的数据来源      |
| `src/HotkeyManager.*`                                              | `RegisterHotKey` 封装（先验证、再替换；ID 用 0x0E45 / 0x0E46）     |
| `src/HotkeyName.*`                                                 | 快捷键格式化、合法性校验、单键白名单                               |
| `src/HotkeyBox.*`                                                  | 设置窗口的快捷键录入控件（自绘、自管焦点与按键捕获）               |
| `src/SettingsDialog.*`                                             | 设置窗口（对话框资源 + 代码设文案）                                |
| `src/TrayIcon.*`                                                   | `Shell_NotifyIcon` 封装、气泡、`TaskbarCreated` 恢复               |
| `src/Config.*` / `src/AutoStart.*`                                 | INI 配置（`%APPDATA%\EasyMute\config.ini`）/ HKCU Run 自启         |
| `src/DebugLog.h`                                                   | 环境变量开关的调试日志（仅 `EASYMUTE_DEBUG=1` 时生效）             |
| `res/EasyMute.rc`、`res/app.manifest`、`res/app.ico`               | 对话框模板、清单（PMv2 DPI / asInvoker / v6 控件）、图标           |
| `tools/selftest.cpp`、`tools/soundstub.cpp`、`tools/make_icon.ps1` | 自检工具 / 测试桩 / 图标生成                                       |

## 架构约定（重要）

1. **单线程 + 消息循环**：全部逻辑跑在主线程；`CoInitializeEx(COINIT_MULTITHREADED)` 在主线程调用一次；按需同步调用 WASAPI，无回调、无常驻线程、无定时器。
2. **热键语义（2026-09-11 / 09-12 需求变更）**：设置快捷键时**直接接受并保存任意合法组合，不再因"被占用"拦截**。注册为尽力而为且完全静默：
   - 注册成功 → 新组合生效；
   - 注册失败（被占用等）→ 保留旧键可用，配置仍保存新值；**界面不做任何提示**（2026-09-12 变更：取消"未生效"标注与占用检测）。
   - 注册流程：直接注销旧键 → 注册新组合；失败时静默恢复旧键（不再使用临时 ID 预验证）。
3. **录入控件与父窗口的协议**：控件捕获到合法组合时，**必须先把候选值（pending）存入控件，再发 `HKN_CHANGED` 通知**；父窗口用 `HKM_GETVALUE` 读取（优先返回候选值），确认后回发 `HKM_SETVALUE`（lParam 保留未用；界面不再标注注册结果）。`HKM_REJECT` 已废弃（常量与处理分支保留以兼容）。
4. **窗口过程协议**：窗口过程及其自定义分发函数在处理 `WM_NCCREATE` / `WM_CREATE` 等创建期消息时，**必须把回调传入的 `hwnd` 透传给 `DefWindowProc`**（不能用成员变量，见"已知坑"1）。
5. **焦点应用匹配策略**：会话命中条件 = `会话 PID ∈ {焦点 PID + 父链 + 全部子进程}` **或** `会话进程 exe 路径 == 焦点进程 exe 路径`（覆盖浏览器多进程 / Electron 子进程等场景）；焦点为 explorer.exe（桌面）时不执行操作；多会话统一取反（要么全静、要么全响）。
6. **设置窗口是模型化对话框**：关闭 = 隐藏；主循环对 `SettingsDialog::Hwnd()` 调用 `IsDialogMessageW`；快捷键控件的 Tab 导航由控件自己用 `WM_NEXTDLGCTL` 转发。
7. **单实例**：命名互斥体 `EasyMute.SingleInstance`；第二实例向 `EasyMuteWindow` 发送注册消息 `EasyMuteOpenSettings` 唤起设置后退出。
8. **托盘**：回调消息 `WM_APP+1`；右键菜单先 `SetForegroundWindow` 再 `TrackPopupMenu`；处理 `TaskbarCreated` 重挂图标。
9. **配置**：INI（`GetPrivateProfile*` 系列 API）；界面修改即时保存；启动时若自启开启则刷新注册表路径（路径自愈）。
10. **文案与编码**：资源脚本纯 ASCII；代码内中文用 `L"..."`；MinGW 宽字符 printf 输出宽字符串必须用 `%ls`（`%s` 是窄字符串语义）。
11. **气泡提示策略（2026-09-12 需求变更）**：仅当静音状态**真正发生切换**时提示一次（"已静音 / 已恢复"）；无音频会话、桌面焦点、解析失败、音频错误等场景**一律静默**（只写调试日志）。提示受设置开关控制（默认开启），并有 300ms 同文本防抖（防重复触发刷屏）。
12. **捕获态热键桥接**：注册中的全局热键会吞掉按键（录入框永远收不到 WM_KEYDOWN，只会触发 WM_HOTKEY）。App 收到 WM_HOTKEY 时先用 `HKM_ISCAPTURING` 询问录入框是否处于捕获态；若是，改调 `SettingsDialog::TryCompleteCaptureWithCurrentHotkey` 以当前快捷键直接完成采集，并阻止一次误静音切换。

## 已知坑（务必避免）

1. **窗口创建期不能用成员 hwnd**（真实踩坑）：`App::HandleMessage` 早期版本用成员 `hwnd_` 调 `DefWindowProc`，导致 `WM_NCCREATE` 被错误处理、`CreateWindowExW` 直接失败（现象：进程活着但无托盘、无热键、卡在报错框）。→ 一律透传回调的 `hwnd`。
2. **快捷键"永远被占用"**（真实踩坑）：`HotkeyBox` 曾只发通知不存候选值，父窗口读到旧键 → 每次改键都在注册旧键 → 任何键都提示"被占用"。→ 先存 pending 再通知（架构约定 3）。
3. **windres 的样式宏**：`res/EasyMute.rc` 必须 `#include <windows.h>`（RC*INVOKED 下仅提供 `WS*\_/DS\_\_`宏），否则报`syntax error`；rc 内不要写中文（按 ANSI/GBK 解析会乱码甚至词法错乱）。
4. **MinGW 默认 manifest 冲突**：GCC 自动附加 `default-manifest.o`，与 `RT_MANIFEST` 冲突（`multiple non-default manifests`）。CMake 已用「生成空 `default-manifest.o` + `-B` 指向空目录」解决，**不要删除该段逻辑**；若构建日志再现该警告，说明覆盖失效，需立即排查。
5. **`RegisterHotKey` 的 ID 范围**：使用 `0x0000–0xBFFF`（当前 0x0E45 / 0x0E46）；`0xC000–0xFFFF` 为 GlobalAddAtom 保留区间。
6. **MinGW `%s` / `%ls`**：宽 printf 中 `%ls` 才是宽字符串；数字格式（`%u` `%zu`）正常。
7. **`MOD_NOREPEAT`**：注册热键必须携带（0x4000），否则长按连发。
8. **测试窗口查找**：PowerShell 里 `FindWindowW(class, $null)` 的 `$null` 会封送成空串而非 NULL，查不到窗口；改用 `EnumWindows` 按类名枚举，或把参数声明为 `IntPtr`。
9. **不要改动静态链接策略**：单文件免安装是产品目标（MinGW 侧 `-static -static-libgcc -static-libstdc++`；MSVC 侧 `/MT`）。
10. **`selftest <pid>` 会做两次切换（静音→恢复）**：净效果还原；调试优先用只读模式 `list` / `config`。
11. **编辑器标红但编译通过（两套 MinGW 默认宏不同）**：IntelliSense 曾把 CompareStringOrdinal / QueryFullProcessImageNameW 等标为“未定义标识符”——IntelliSense 解析所用工具链默认 `_WIN32_WINNT` 过低时（如旧 MinGW GCC 8.1 = 0x0502），头文件把 Vista+ API 声明藏在 `#if WINVER >= 0x0600` 守卫里；而构建工具链（WinGet WinLibs GCC 16.1 = 0x0601）正常。解决：**机器相关的 `.vscode/c_cpp_properties.json` 保持本地、不入库**（compilerPath 指向本人构建工具链 + 显式 `WINVER/_WIN32_WINNT=0x0601`；模板见 README「VS Code 编辑器提示」）；CMake 已显式定义这两个宏，构建不再依赖工具链默认值。**教训：含本机绝对路径的编辑器配置不要提交。**

## 测试技巧

- **造测试目标**：`easymute_soundstub.exe` 渲染静音数据、产生自己的音频会话；它带同名窗口，可 `SetForegroundWindow` 后做真按键测试。
- **真按键仿真**：聚焦目标窗口后用 `SendKeys '^%{F9}'`（PowerShell）触发全局热键；用 `selftest list` 的 `muted=是/否` 验证结果。
- **热键占用探测**：P/Invoke `RegisterHotKey(IntPtr.Zero, id, mods, vk)`——返回 `False` 表示组合已被占用（即我们的程序持有）；结束后记得 `UnregisterHotKey`。
- **单实例验证**：连续启动两个进程，第二个应在 2 秒内退出（exit=0）。
- **设置流程验证**：启动主程序 → 再启动一个实例（自动唤起设置窗口）→ 向录入框发送 `WM_LBUTTONDOWN` 聚焦 → `SendKeys` 输入组合 → 检查 `%APPDATA%\EasyMute\config.ini` 与热键占用状态。
- **清理**：测试结束后结束 `EasyMute` / `easymute_*` 进程，并删除临时 `config.ini`，避免影响下一次测试。

## 经验更新日志

- **2026-09-11 初版创建**：汇总 v1.0.0 开发全程经验。当天两个最大的坑：「窗口创建期使用成员 hwnd 导致创建失败」与「录入控件未保存候选值导致改键永远提示被占用」（均已修复，见"已知坑"1/2）。
- **2026-09-11 需求变更**：设置快捷键不再提示 / 拦截"被占用"，直接接受任意合法组合（注册失败则保留旧键可用，托盘提示标注"未生效"）。连带调整：启动时不再弹"注册失败"气泡；`HKM_REJECT` 废弃。
- **2026-09-12 气泡策略修复**：现象——日常误触快捷键时气泡"一直不断弹出"（无音频会话 / 桌面焦点等未切换场景也在弹，且强制显示、无视设置）。根因——通知策略未区分"是否真正切换静音"。修复——仅切换成功时提示一次 + 300ms 同文本防抖 + 全部非切换场景静默；设置开关更名为"静音/恢复时显示气泡提示"（默认开启）。
- **2026-09-12 修复「恢复默认假成功」与「按当前快捷键无反应」**：现象1——点“恢复默认”后输入框显示已切到 `Ctrl + Alt + M`，实际未生效（该组合在真机上被其他程序占用，注册返回 err=1409），界面却按“配置值”显示，形成假成功；现象2——录入时按下“当前正在使用的快捷键”，输入框毫无反应。根因——(1) 输入框显示只取配置值，未区分是否注册成功；(2) 全局热键会吞掉按键，录入框收不到 WM_KEYDOWN，只会触发 WM_HOTKEY。结论——显示与提示统一以“实际注册结果”为准（未生效红色标注）；捕获态下 WM_HOTKEY 交由设置窗口完成采集（同一按键直接回显当前快捷键）。经截图 + 日志端到端验证。
- **2026-09-12 需求变更（第三次：彻底静默）**：用户要求去掉“（未生效）”红色标注、不再做任何占用检测——“就让用户能直接设置”。实现——`HotkeyBox` 删除 `effective` 字段与红色标注分支；`App::UpdateTrayTip` / `IsHotkeyEffective` 移除；`HKM_SETVALUE` 的 lParam 语义废弃（保留未用）；`SettingsDialog::SyncHotkey` 回到 2 参数；`HotkeyManager::Apply` 移除临时 ID 预验证，改为直接注册、失败静默恢复旧键。验证——占用组合（真机 Ctrl+Alt+M，err=1409）下界面普通显示、无任何提示；F8 正常录入注册；捕获态按当前快捷键仍可回显完成采集（截图 + 日志）。
- **2026-09-12 修复编辑器标红（体验类）**：现象——部分 .cpp 中 QueryFullProcessImageNameW / CompareStringOrdinal 等被编辑器标为未定义，但编译运行正常。根因——IntelliSense 与实际构建使用不同工具链（旧 MinGW GCC 8.1 默认 `_WIN32_WINNT=0x0502` vs WinGet WinLibs GCC 16.1 默认 `0x0601`），宏低于 0x0600 时头文件的 `#if WINVER >= 0x0600` 守卫隐藏声明。结论——本地新增 `.vscode/c_cpp_properties.json`（compilerPath 指向构建工具链 + 显式 WINVER/\_WIN32_WINNT=0x0601）；CMakeLists 同步显式定义这两个宏；验证：IntelliSense 对相关文件 0 错误。后补——该文件含本机绝对路径、曾误入库，经确认对他人不可移植（clone 后路径不存在，会覆盖并弄坏其编辑器体验），已回退为“本地文件 + `.gitignore` 忽略”，并**改写 git 历史**（合并重写相关提交 + 强推）从仓库历史中彻底移除该文件及排查文本中的机器路径；模板写入 README。
- （后续新经验请在此追加：日期 + 现象 + 根因 + 结论）
