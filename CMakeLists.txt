cmake_minimum_required(VERSION 3.20)

project(EasyMute
    VERSION 1.0.0
    DESCRIPTION "Windows 轻量托盘工具：全局热键一键静音当前焦点应用"
    LANGUAGES CXX RC
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC：静态链接 CRT，生成无运行时依赖的单文件
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 源码为 UTF-8（含中文字符串字面量）
if(MSVC)
    add_compile_options(/utf-8)
endif()

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

# ---------- 核心库（主程序与自检工具共用） ----------
add_library(easymute_core STATIC
    src/AppResolver.cpp
    src/AudioSession.cpp
    src/AutoStart.cpp
    src/Config.cpp
    src/HotkeyName.cpp
)
target_include_directories(easymute_core PUBLIC src)
target_link_libraries(easymute_core PUBLIC ole32)

# ---------- 主程序（GUI，无控制台窗口） ----------
add_executable(EasyMute WIN32
    src/App.cpp
    src/HotkeyBox.cpp
    src/HotkeyManager.cpp
    src/SettingsDialog.cpp
    src/TrayIcon.cpp
    src/main.cpp
    res/EasyMute.rc
)
target_include_directories(EasyMute PRIVATE src res)
target_link_libraries(EasyMute PRIVATE
    easymute_core
    user32 shell32 gdi32 comctl32 advapi32
)
# 图标变化时重新编译资源
set_property(SOURCE res/EasyMute.rc PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/app.ico")

# ---------- 自检工具（控制台，验证“前台进程 → 音频会话 → 静音”链路） ----------
add_executable(easymute_selftest tools/selftest.cpp)
target_link_libraries(easymute_selftest PRIVATE easymute_core)

# ---------- 静音测试桩（控制台，在默认设备上制造一个真实音频会话） ----------
add_executable(easymute_soundstub tools/soundstub.cpp)
target_link_libraries(easymute_soundstub PRIVATE easymute_core)

if(MINGW)
    target_link_options(EasyMute PRIVATE -municode -mwindows -static -static-libgcc -static-libstdc++)
    target_link_options(easymute_selftest PRIVATE -municode)
    target_link_options(easymute_soundstub PRIVATE -municode)

    # MinGW 的 GCC 会自动附加 default-manifest.o（内含 longPathAware 的默认清单），
    # 与我们在 .rc 中嵌入的清单冲突（链接警告 + 双清单）。
    # 办法：生成一个空的同名对象，用 -B 让 GCC 的文件搜索优先找到它，从而不附加默认清单。
    set(EASYMUTE_EMPTY_CRT_DIR ${CMAKE_BINARY_DIR}/empty-crt)
    file(MAKE_DIRECTORY ${EASYMUTE_EMPTY_CRT_DIR})
    file(WRITE ${EASYMUTE_EMPTY_CRT_DIR}/empty.cpp "")
    add_custom_command(
        OUTPUT ${EASYMUTE_EMPTY_CRT_DIR}/default-manifest.o
        COMMAND ${CMAKE_CXX_COMPILER} -c ${EASYMUTE_EMPTY_CRT_DIR}/empty.cpp -o ${EASYMUTE_EMPTY_CRT_DIR}/default-manifest.o
        DEPENDS ${EASYMUTE_EMPTY_CRT_DIR}/empty.cpp
        COMMENT "生成空的 default-manifest.o（覆盖 MinGW 默认清单）")
    add_custom_target(easymute_empty_manifest DEPENDS ${EASYMUTE_EMPTY_CRT_DIR}/default-manifest.o)
    add_dependencies(EasyMute easymute_empty_manifest)
    target_link_options(EasyMute PRIVATE "-B${EASYMUTE_EMPTY_CRT_DIR}/")
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT EasyMute)
