# 天地飞 ET08A 串口模拟器

C++20 + **Slint** 桌面界面，使用 **libserialport** 发送与参考工程兼容的 SBUS 帧。应用代码、协议编码和发送线程均为 C++；Slint 使用 `.slint` 文件描述界面。支持面向 Windows、Linux、macOS 构建。

## 功能

- 双摇杆拖动、四轴滑块与数值输入，可分别选择松手回中或保持位置。
- SA、SD 两档，SB、SC 三档，四轴一键回中。
- 枚举串口，也可手动输入 `COM3`、`/dev/ttyUSB0`、`/dev/cu.usbserial-...`。
- 默认 70 Hz，支持 10～100 Hz，独立线程周期发送，停止时关闭串口。
- 实时预览前八通道原始值、完整 25 字节十六进制帧、已提交帧数。
- 模拟接收器失联，可单独设置 SBUS 的 frame-lost、failsafe 位。
- 串口打开、配置、写入失败时停止发送并显示原因；可修复连接后重新开始。

## 构建图形程序

需要 CMake 3.21+、支持 C++20 的编译器和 libserialport 开发包。CMake 优先寻找已安装的 Slint 1.18+；找不到时会下载并编译固定版本 **Slint 1.18.1**。从源码构建 Slint 还需要 Rust 1.92+、Cargo、Git 和网络。

当前工作区已完成 Linux x86-64 Release 构建，可直接运行：

```sh
./build/et08a_emulator
```

本地 Slint SDK 位于 `.deps/slint/Slint-cpp-1.18.1-Linux-x86_64`，下载文件的 SHA-256 已与官方发布元数据核对。运行时需要保留该目录。重新配置和构建可使用：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$PWD/.deps/slint/Slint-cpp-1.18.1-Linux-x86_64" -DET08_FETCH_SLINT=OFF
cmake --build build --parallel
```

### Linux

Debian / Ubuntu 示例：

```sh
sudo apt install build-essential cmake ninja-build git pkg-config libserialport-dev \
    libfontconfig1-dev libfreetype6-dev libxkbcommon-dev libwayland-dev libudev-dev
```

Arch Linux 的串口依赖为 `libserialport`，构建工具可使用系统的 `base-devel`、`cmake`、`ninja`、`rust`、`git`。Slint 的窗口后端依赖参见下方官方文档。

安装合适版本的 Rust 后，在项目目录执行：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/et08a_emulator
```

如果串口报权限不足，需要当前用户拥有该串口设备的读写权限；常见设备组为 `dialout` 或 `uucp`，以系统实际设置为准。

### Windows

使用 Visual Studio 2022 的 x64 开发者终端，安装 CMake、Ninja、Git、Rust 和 vcpkg。工程自带 `vcpkg.json`，配置时通过 vcpkg 安装 libserialport：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --parallel
.\build\et08a_emulator.exe
```

将 `C:/src/vcpkg` 替换为实际目录。使用预编译 Slint 时，选择与编译器和架构匹配的 C++ SDK。构建会复制能识别到的运行库 DLL；分发时需要同时携带 libserialport、Slint 及其所需的运行库。

### macOS

安装 Xcode Command Line Tools、Rust 和依赖：

```sh
brew install cmake ninja libserialport
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/et08a_emulator
```

### 已安装 Slint / 离线构建

如果已有 Slint C++ SDK，用安装前缀指定它，不再下载 Slint：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/path/to/slint-sdk -DET08_FETCH_SLINT=OFF
cmake --build build --parallel
```

libserialport 也需要提前安装。非标准安装目录可以加入 `CMAKE_PREFIX_PATH`；也可明确传入 `SERIALPORT_INCLUDE_DIR` 和 `SERIALPORT_LIBRARY`。使用共享 Slint SDK 时，按其文档设置 `PATH` / `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` 使系统能找到运行库。

## 使用

1. 连接 USB 转 TTL 串口，刷新并选择设备，或在输入框手动填写设备名。
2. 调整摇杆和拨杆；此时只有帧预览，不会写串口。
3. 点击“开始发送”，以所选频率打开串口并发送当前状态。连接期间仍可实时操作摇杆和拨杆。
4. 点击“停止发送”或关闭窗口，停止周期发送并释放串口。参考工程约 100 ms 收不到帧会触发离线。

摇杆向右、向上为正值。数值框表示**减去 1024 后的偏移量**，滑块 / 数值输入会保持设定值；“鼠标松开后回中”只作用于摇杆拖动。

“模拟接收器失联”会覆盖摇杆和拨杆的发送值：四轴回中，SA～SD 全为中位。取消后恢复界面上保留的状态。Frame lost / Failsafe 仅修改 SBUS 状态字节，**参考驱动目前没有读取这两个标志**，它们不会单独让 `ET08_IsOnline()` 返回 false。

“已提交帧”指完整提交给串口驱动的帧数，不代表下位机已确认接收。桌面系统和 USB 串口存在调度误差，70 Hz 是目标周期。线程使用单调时钟；错过周期后跳过补发，不会突发发送积压帧。写入超时或输出队列未及时清空时停止发送，避免持续积压旧的控制量。

## 接线与串口参数

参考工程 `Core/Src/usart.c` 的 `MX_UART5_Init()`：

| 参数 | PC 端配置 |
| --- | --- |
| 波特率 | 100000 |
| 数据位 | 8 |
| 校验 | 偶校验 Even |
| 停止位 | 2 |
| 流控 | 无 |
| 默认周期 | 约 14.286 ms（70 Hz） |

STM32 配置是 `UART_WORDLENGTH_9B` + `UART_PARITY_EVEN`，其中一位用于校验，所以电脑端配置为 **8E2**，不是九个数据位。

参考工程 UART5_RX 为 **PD2**，UART5 没有启用片内 RX 反相。若接到 MCU 的 UART RX 侧，可用兼容其电平的 USB-TTL **TX → UART5_RX / PD2，GND → GND**；本程序只发送，不需要接电脑端 RX。使用 TTL 电平串口，不使用 RS-232 电平。

标准 SBUS 物理信号为反相逻辑。本程序通过普通串口发出正常 UART 电平；如果接入开发板上带硬件反相器的 SBUS 接收器接口，需要匹配该接口的极性，或接到反相器后的 MCU UART RX 侧。参考源码无法确定开发板外部反相电路，接线应结合原理图确认。对帧字节按位取反不能代替物理电平反相，因为起始位和停止位同样涉及极性。

USB 串口芯片与驱动必须支持 **100000 波特率和 8E2**；配置失败会显示错误，不会降级为其他波特率。

## 协议对应关系

依据：`chassis/dr_nova_framework/Device/Input/Src/wefly_et08a.c`、`Inc/wefly_et08a.h`，以及 `chassis/Core/Src/usart.c`。

| 通道 | 参考驱动字段 | 发送值 |
| --- | --- | --- |
| CH1 | `rocker_r_`，右水平 | 偏移量 + 1024 |
| CH2 | `rocker_r1`，右竖直 | 偏移量 + 1024 |
| CH3 | `rocker_l1`，左竖直 | 偏移量 + 1024 |
| CH4 | `rocker_l_`，左水平 | 偏移量 + 1024 |
| CH5 | SA | 上 353，下 1694 |
| CH6 | SB | 上 353，中 1024，下 1694 |
| CH7 | SC | 上 353，中 1024，下 1694 |
| CH8 | SD | 上 353，下 1694 |
| CH9～16 | 参考驱动未使用 | 固定 1024 |

摇杆原始值为 **353～1694**，中位 **1024**，所以偏移范围是 **-671～+670**。参考驱动拨杆枚举：上=1、中=3、下=2。

帧格式为：`0F` + 22 字节通道载荷（16 × 11 bit，低位优先连续打包）+ 1 字节状态 + `00`。状态字节 bit2 为 frame-lost，bit3 为 failsafe；数字 CH17/18 置零。参考驱动只提取前 8 个通道，帧头尾和未使用通道采用常见 SBUS 格式。

默认四轴回中、四拨杆在上档时：

```text
0F 00 04 20 00 01 18 96 B0 84 25 2C 00 04 20 00 01 08 40 00 02 10 80 00 00
```

## 无 UI 依赖的验证

协议和发送线程可单独编译，不需要 Slint、Rust 或 libserialport，也不会访问硬件：

```sh
cmake -S . -B build-core -G Ninja -DET08_BUILD_APP=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
./build-core/et08a_frame --rx -671 --ly 670
./build-core/et08a_frame --offline
```

`et08a_frame` 只打印单帧，用 `--help` 查看参数。Windows 下使用对应的 `.exe`。

测试包含默认 / 离线帧固定样例、与参考解码表达式逐轴对照的 5368 个值、36 种拨杆组合、状态位和 2000 组完整载荷，以及模拟串口上的周期发送、状态更新、停止、异常恢复和错过周期处理。

本次环境已完成 Slint 1.18.1 + libserialport 0.1.2 的完整 Linux x86-64 Release 构建，协议与发送线程测试全部通过（2/2）。程序启动运行无错误，已通过实际窗口渲染及截图检查。构建时修复了 Slint 窗口同时声明固定尺寸与最小尺寸的冲突，改用首选尺寸。另直接抽取参考工程中未修改的 `ET08_Decode()` / `ET08_ValidateRocker()` 编译比对，5404 个摇杆 / 拨杆案例全部通过。**尚未进行真实硬件串口联调，也未在 Windows / macOS 上验证构建。**

## 文件与依赖文档

- `ui/app.slint`：Slint 界面与摇杆交互。
- `src/main.cpp`：界面绑定、串口枚举和状态刷新。
- `src/protocol.cpp`：ET08A 通道映射、SBUS 编码。
- `src/sender.cpp`：独立发送线程和生命周期。
- `src/serial_port.cpp`：libserialport 封装。
- `tests/`：不依赖图形环境和串口硬件的测试。

[Slint C++ 构建文档](https://docs.slint.dev/latest/docs/cpp/cmake/)、[Slint 桌面后端依赖](https://docs.slint.dev/latest/docs/slint/guide/platforms/desktop/)、[libserialport 官方介绍](https://sigrok.org/wiki/Libserialport)、[SBUS 格式说明](https://github.com/bolderflight/sbus)。

libserialport 使用 LGPL-3.0-or-later，Slint 提供多种许可选项；分发程序时请遵循所选依赖的许可证。
