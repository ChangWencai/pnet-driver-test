# p-net IO Device Manager

基于 [p-net](https://github.com/rtlabs-com/p-net) 开源协议栈的 Profinet IO Device 管理程序，面向 Linux 工业控制场景，提供完整的网络配置、安全策略、实时调度和 IO 设备生命周期管理。

## 功能特性

- **Profinet IO Device 生命周期管理** — 初始化、连接、数据交换、诊断、告警、优雅停机
- **网络接口配置** — 基于 Linux ioctl 的 IP/子网掩码设置，TCP 参数调优（/proc/sys）
- **安全策略管理** — iptables 防火墙规则生成与执行、IPSec 隧道（strongSwan）、安全审计
- **实时环境配置** — SCHED_FIFO 调度策略、mlockall 内存锁定、CPU 隔离、延迟测量
- **灵活构建模式** — Mock 模式用于跨平台开发测试，Real 模式对接真实 p-net 协议栈
- **systemd 服务集成** — 提供完整的 service 文件、sysctl 调优和安全加固配置
- **key=value 配置文件** — 支持注释、默认值回退、运行时验证

## 项目结构

```
pnet-driver-test/
├── CMakeLists.txt          # CMake 构建配置（支持 Mock / Real p-net 双模式）
├── Makefile                # CMake 快捷封装
├── .gitmodules             # p-net 子模块定义
│
├── include/                # 公共头文件
│   ├── io_device.h         #   IO Device 生命周期与数据交换
│   ├── network_config.h    #   网络接口配置与 TCP 调优
│   ├── security_policy.h   #   防火墙规则、IPSec、安全审计
│   ├── rt_setup.h          #   实时调度与内存锁定
│   ├── app_config.h        #   配置文件解析与验证
│   ├── pnal_config.h       #   p-net 平台抽象层（Linux POSIX 类型定义）
│   └── pnal_sys.h          #   系统级平台适配
│
├── src/                    # 源文件
│   ├── main.c              #   入口：参数解析 → 加载配置 → 初始化子系统 → 主循环
│   ├── app/io_device.c     #   IO Device 实现（对接 p-net API 或 mock）
│   ├── net/network_config.c#   网络配置（ioctl / getifaddrs / /proc/sys）
│   ├── security/security_policy.c  # 安全策略（iptables 脚本生成与执行）
│   ├── realtime/rt_setup.c #   实时配置（sched_setscheduler / mlockall）
│   └── config/app_config.c #   配置文件解析器
│
├── tests/                  # 测试套件
│   ├── test_framework.h    #   轻量测试框架（ASSERT 宏、彩色输出）
│   ├── test_main.c         #   测试入口，运行 5 个测试套件
│   ├── test_io_device.c    #   IO Device 测试（8 项）
│   ├── test_network.c      #   网络配置测试（7 项）
│   ├── test_security.c     #   安全策略测试（7 项）
│   ├── test_realtime.c     #   实时配置测试（6 项）
│   ├── test_config.c       #   配置文件测试（5 项）
│   └── mock/               #   p-net API Mock 实现
│       ├── pnet_api_mock.h
│       └── pnet_api_mock.c
│
├── deploy/                 # 部署配置
│   ├── pnet-manager.service    # systemd 服务文件
│   ├── pnet-manager.conf       # 默认配置文件
│   └── 99-pnet-tuning.conf     # sysctl TCP 调优参数
│
├── scripts/                # 构建脚本
│   ├── build_pnet.sh       #   编译 p-net 协议栈
│   └── setup_env.sh        #   开发环境检查
│
└── vendor/p-net/           # p-net 协议栈（git submodule）
```

## 环境要求

**必需：**

| 依赖 | 最低版本 | 说明 |
|------|---------|------|
| GCC / Clang | GCC 7+ / Clang 5+ | C11 标准支持 |
| CMake | 3.14+ | 构建系统 |
| Make | 任意 | 编译驱动 |
| Linux Kernel | 4.x+ | ioctl、/proc/sys、SCHED_FIFO 支持 |

**可选：**

| 工具 | 用途 |
|------|------|
| PREEMPT_RT 内核 | 硬实时场景 |
| strongSwan / libreswan | IPSec 隧道 |
| iptables / nftables | 防火墙规则执行 |
| valgrind | 内存泄漏检测 |

## 快速开始

### 1. 克隆项目

```bash
git clone --recursive https://github.com/ChangWencai/pnet-driver-test.git
cd pnet-driver-test
```

### 2. Mock 模式构建（开发 / 测试）

Mock 模式使用内置的 p-net API 模拟层，无需真实 p-net 源码，可在 macOS 和 Linux 上编译运行。

```bash
# 方式一：使用 Makefile 快捷命令
make          # 编译
make test     # 编译并运行测试
make run      # 编译并启动程序（verbose 模式）

# 方式二：手动 CMake（推荐）
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)
./build/pnet-tests    # 运行测试
./build/pnet-manager -v  # 启动程序
```

### 3. Real p-net 模式构建（生产部署）

Real 模式需要预编译的 p-net 静态库（`libpnet.a`），由 `scripts/build_pnet.sh` 构建：

```bash
# 第一步：构建 p-net 协议栈（需要完整的 p-net 商业版源码）
./scripts/build_pnet.sh

# 第二步：构建主项目
cmake -B build -DUSE_REAL_PNET=ON -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

> **注意：** 当前 `vendor/p-net` 子模块为精简评估版，不包含 CMakeLists.txt、OSAL 抽象层和构建系统。如需完整版本（含 OSAL），请参考 [rt-labs 官方文档](https://rt-labs.com/docs/p-net) 获取商业授权。评估模式下请使用 Mock 模式（`-DUSE_REAL_PNET=OFF`）进行开发和测试。

## 命令行参数

```
Usage: pnet-manager [OPTIONS]

Options:
  -c <file>   配置文件路径（默认: /etc/pnet-manager/config.conf）
  -v          详细输出（日志级别设为 DEBUG）
  -h          显示帮助信息

Signals:
  SIGINT, SIGTERM   优雅停机
```

## 配置文件

默认配置文件位于 `/etc/pnet-manager/config.conf`，采用 `key = value` 格式，支持 `#` 和 `;` 注释。

```ini
# --- 设备标识 ---
product_name = p-net IO Device
station_name = iodevice1
vendor_id = 0x0001
device_id = 0x0001

# --- 网络 ---
interface = eth0
ip_addr = 192.168.0.100
netmask = 255.255.255.0
gateway = 192.168.0.1

# --- Profinet 协议栈 ---
tick_us = 1000          # 主循环 tick 间隔（微秒）
send_hello = 1

# --- 实时配置 ---
realtime_enabled = 1
rt_priority = 80        # SCHED_FIFO 优先级 (1-99)
lock_memory = 1         # mlockall 防止缺页中断
isolate_cpu = -1        # CPU 核隔离（-1 = 不隔离）

# --- 安全 ---
security_enabled = 1

# --- 日志 ---
log_level = 2           # 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG
log_file = /var/log/pnet-manager/pnet-manager.log

# --- 存储 ---
file_directory = /var/lib/pnet-manager
```

## 生产部署

### 安装

```bash
cd build
cmake --install . --prefix /usr/local
```

安装目标：

| 路径 | 文件 |
|------|------|
| `/usr/local/bin/pnet-manager` | 主程序 |
| `/usr/local/lib/systemd/system/pnet-manager.service` | systemd 服务 |
| `/usr/local/etc/pnet-manager/pnet-manager.conf` | 默认配置 |

### 部署配置文件

```bash
# 创建运行目录
mkdir -p /var/lib/pnet-manager /var/log/pnet-manager

# 安装配置文件
cp deploy/pnet-manager.conf /etc/pnet-manager/config.conf
cp deploy/99-pnet-tuning.conf /etc/sysctl.d/

# 应用 TCP 调优
sysctl -p /etc/sysctl.d/99-pnet-tuning.conf
```

### systemd 服务管理

```bash
# 启用并启动
systemctl daemon-reload
systemctl enable --now pnet-manager

# 查看状态
systemctl status pnet-manager

# 查看日志
journalctl -u pnet-manager -f
```

服务文件已包含安全加固配置：`ProtectSystem=strict`、`PrivateTmp=true`、`ReadWritePaths` 限制、RT 调度权限和内存锁定权限。

## 模块说明

### IO Device（`src/app/io_device.c`）

管理 Profinet IO Device 的完整生命周期。在 Real 模式下对接 `pnet_init()` / `pnet_handle_periodic()` 等 API，通过回调机制处理连接建立、数据交换和告警。在 Mock 模式下使用内置模拟层，支持本地开发和测试。

默认插入 DAP 模块（slot 0）和一个用户 I/O 模块（slot 1），支持输入/输出数据读写。

### Network Config（`src/net/network_config.c`）

基于 Linux `ioctl`（SIOCGIFADDR / SIOCSIFADDR）配置网络接口 IP 和子网掩码，通过 `getifaddrs` 获取接口信息。TCP 调优通过读写 `/proc/sys/net/ipv4/` 下的参数文件实现。macOS 平台使用 `getifaddrs` 兼容实现。

### Security Policy（`src/security/security_policy.c`）

管理防火墙规则集合，支持添加、删除、清空操作。`sec_policy_apply()` 生成 iptables 脚本并通过 `system()` 执行。支持 IPSec 隧道管理（生成 strongSwan 配置文件）和安全策略审计。

### Real-Time Setup（`src/realtime/rt_setup.c`）

配置 Linux 实时环境：`sched_setscheduler` 设置 SCHED_FIFO 调度策略，`mlockall` 锁定内存防止缺页中断，支持 CPU 频率调节器关闭和 irqbalance 服务管理。提供延迟测量功能用于验证实时性能。所有操作均有 `#ifdef PLATFORM_LINUX` 保护。

### App Config（`src/config/app_config.c`）

key=value 格式配置文件解析器，支持 `#` 和 `;` 注释，自动跳过空行。提供加载、保存、默认值填充和校验功能。校验项包括 tick 间隔、IP 地址格式、优先级范围等。

## 测试

项目包含 33 个单元测试，覆盖全部 5 个功能模块：

```bash
# 运行全部测试
make test

# 或通过 CTest
cd build && ctest --output-on-failure
```

| 测试套件 | 测试数量 | 覆盖内容 |
|---------|---------|---------|
| IO Device | 8 | 初始化、生命周期、数据交换、状态管理 |
| Network | 7 | IP 转换/校验、接口配置、TCP 调优 |
| Security | 7 | 规则增删、脚本生成、审计、策略应用 |
| Real-Time | 6 | 配置默认值、初始化、延迟测量、应用/恢复 |
| Config | 5 | 默认值、校验、持久化往返、异常处理 |

## CMake 构建选项

| 选项 | 默认值 | 说明 |
|------|-------|------|
| `USE_REAL_PNET` | `OFF` | 启用真实 p-net 协议栈（需要 Linux） |
| `BUILD_TESTS` | `ON` | 构建测试套件 |
| `CMAKE_BUILD_TYPE` | 无 | `Debug`（含 ASan）或 `Release` |

```bash
# Debug 构建（含 AddressSanitizer，仅 Linux）
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Release 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 仅构建主程序，跳过测试
cmake -B build -DBUILD_TESTS=OFF
```

## 开发环境检查

```bash
./scripts/setup_env.sh
```

检查 GCC、CMake、Git、Make 等必需工具，以及 iftop、nethogs、valgrind 等可选工具的安装状态，并显示可用网络接口。

## 许可证

本项目代码采用 MIT 许可证。

p-net 协议栈（`vendor/p-net/`）采用 GPLv3 和商业双许可证。如在商业产品中使用，请联系 [rt-labs](https://rt-labs.com) 获取商业授权。
