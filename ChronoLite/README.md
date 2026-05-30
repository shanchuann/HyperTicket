# ChronoLite

轻量级 C++ 时间戳与日志系统库，提供微秒级时间戳、格式化输出、时间运算以及可扩展的日志输出接口，适用于日志记录、性能分析、文件命名等场景。

## 功能概述

- 获取当前时间戳（微秒级精度）
- 多种时间格式化输出（标准/文件友好型/秒.微秒）
- 时间戳有效性判断、差值计算、加减运算
- 日志系统支持多级别（TRACE/DEBUG/INFO/WARN/ERROR/FATAL）
- 日志输出可自定义（控制台/文件/网络）
- 跨平台支持：Windows & Linux

## 文件结构

```
ChronoLite/
├── logsys/
│   ├── include/
│   │   ├── Timestamp.hpp    # 时间戳类声明
│   │   ├── LogMessage.hpp   # 日志消息类声明
│   │   ├── Logger.hpp       # 日志核心类声明
│   │   ├── LogCommon.hpp    # 日志等级定义
│   └── src/
│       ├── Timestamp.cpp    # 时间戳实现
│       ├── LogMessage.cpp   # 日志消息实现
│       └── Logger.cpp       # 日志核心实现
├── test.cpp                 # 测试程序（示例/单文件演示）
├── CMakeLists.txt           # CMake 构建配置
├── .gitignore               # 忽略 build/ bin/
└── README.md                # 项目说明文档
```

## 编译与运行

1. 安装 CMake（≥3.10）和 C++ 编译器（g++/MSVC）
2. 创建并进入 build 目录：
   ```bash
   mkdir build && cd build
   ```
3. 配置项目：
   ```bash
   cmake ..
   ```
4. 编译项目：
   ```bash
   make
   ```
5. 运行测试：
   ```bash
   ./bin/chronolite
   ```
   
快速复制运行（Linux 示例）：

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./bin/chronolite
```

## API 说明

以下为快速 API 参考，包含主要方法签名与最小示例，方便快速查阅。更详细的用法请参考源文件及示例代码。

#### 快速签名与示例

```cpp
// Timestamp
class Timestamp {
public:
   Timestamp();                          // 无效时间戳
   Timestamp(int64_t microSeconds);
   static Timestamp Now();               // 当前时间（微秒）
   std::string toString() const;         // 秒.微秒
   std::string toFormattedString(bool showMic=true) const; // YYYY/MM/DD HH:MM:SS[.微秒]
   std::string toFileString() const;     // 文件友好型 YYYYMMDD-HHMMSS[.微秒]
   int64_t getMicroSec() const;
   time_t getSeconds() const;
};

// LogMessage
class LogMessage {
public:
   LogMessage(LOG_LEVEL level, const std::string& file, const std::string& func, int line);
   ~LogMessage();
   std::string toString() const;
   LogMessage& operator<<(const std::string& v);
};

// Logger
class Logger {
public:
   using OutputFun = std::function<void(const std::string&)>;
   using FlushFun = std::function<void()>;
   static void SetOuput(OutputFun out);   // 注意源码拼写为 SetOuput
   static void SetFlush(FlushFun flush);
   static void SetLogLevel(const LOG_LEVEL&);
   Logger(LOG_LEVEL level, const std::string& file, const std::string& func, int line);
   ~Logger(); // 析构时会触发输出
};

// LogFile (文件输出、滚动)
class LogFile {
public:
   LogFile(const std::string& basename, size_t rollSize=1024*128, int flushInterval=3, int checkEventN=30, bool threadSafe=true);
   ~LogFile();
   void append(const std::string& msg);
   void flush();
};

// 简单示例：设置文件输出
// logsys::Logger::SetOuput(fileOutput);
// logsys::Logger::SetFlush(fileFlush);
// pfile = new logsys::LogFile("logfile_test");
```

### `Timestamp` 类

| 方法名 | 功能描述 |
|--------|----------|
| `Timestamp()` | 构造无效时间戳 |
| `Timestamp(int64_t ms)` | 构造指定微秒数时间戳 |
| `~Timestamp()` | 析构函数 |
| `void swap(Timestamp& other)` | 交换时间戳 |
| `bool vaild() const` | 判断有效性（注意：方法名为 `vaild()`，与常见拼写 `valid()` 不同，接口以源码为准） |
| `std::string toString() const` | 秒.微秒格式字符串 |
| `std::string toFormattedString(bool showMic = true) const` | 标准格式化字符串 |
| `std::string toFileString() const` | 文件友好型字符串 |
| `int64_t getMicroSec() const` | 获取微秒数 |
| `time_t getSeconds() const` | 获取秒数 |
| `static Timestamp Now()` | 当前时间戳（微秒级） |
| `static Timestamp invalid()` | 无效时间戳 |
| `diffSeconds(const Timestamp&, const Timestamp&)` | 秒差 |
| `diffMicroSeconds(const Timestamp&, const Timestamp&)` | 微秒差 |
| `addTime(const Timestamp&, double seconds)` | 增加秒数 |

### `LogMessage` 类

| 方法名 | 功能描述 |
|--------|----------|
| `LogMessage(LOG_LEVEL, const std::string&, const std::string&, int)` | 构造日志消息 |
| `~LogMessage()` | 析构 |
| `const LOG_LEVEL& getLogLevel() const` | 获取日志等级 |
| `void setLogLevel(const LOG_LEVEL&)` | 设置日志等级 |
| `const std::string toString() const` | 获取日志字符串 |
| `operator<<` | 流式插入内容 |

### `Logger` 类

| 方法名 | 功能描述 |
|--------|----------|
| `Logger(LOG_LEVEL, const std::string&, const std::string&, int)` | 构造日志对象 |
| `~Logger()` | 析构自动输出日志 |
| `LogMessage& stream()` | 获取日志流 |
| `static void SetOuput(OutputFun)` | 设置输出函数 |
| `static void SetFlush(FlushFun)` | 设置刷新函数 |
| `static LOG_LEVEL getLogLevel()` | 获取日志等级 |
| `static void SetLogLevel(const LOG_LEVEL&)` | 设置日志等级 |

### `LogFile` 类
| 方法名 | 功能描述 |
|--------|----------|
| `LogFile(const std::string&, size_t, int, int, bool)` | 构造日志文件对象 |
| `~LogFile()` | 析构 |
| `void append(const std::string&)` | 追加日志内容 |
| `void flush()` | 刷新日志内容 |  

### `LOG_LEVEL` 枚举
| 枚举值 | 描述 |
|--------|------|
| `TRACE` | 跟踪信息 |
| `DEBUG` | 调试信息 |
| `INFO`  | 一般信息 |
| `WARN`  | 警告信息 |
| `ERROR` | 错误信息 |
| `FATAL` | 致命错误 |

## 扩展用法

### 自定义日志输出

```cpp
#include "Logger.hpp"
#include <fstream>

std::ofstream g_logfile("chronolite.log", std::ios::app);
void myOutput(const std::string& msg) {
   if (g_logfile) g_logfile << msg;
}
void myFlush() {
   if (g_logfile) g_logfile.flush();
}
int main() {
   logsys::Logger::SetOuput(myOutput);
   logsys::Logger::SetFlush(myFlush);
   LOG_INFO << "自定义日志输出到 chronolite.log";
   return 0;
}
```

### 日志等级控制

可通过环境变量或 Logger::SetLogLevel 控制日志输出等级，便于调试和生产环境切换。

### 时间戳差值与运算

支持 diffSeconds、diffMicroSeconds、addTime 等时间戳运算，适用于性能分析、事件间隔统计等场景。

## 使用示例

```cpp
#include "Timestamp.hpp"
#include <iostream>
int main() {
    logsys::Timestamp t = logsys::Timestamp::Now();
    std::cout << t.toFormattedString() << std::endl;
    std::cout << t.toFileString() << std::endl;
}
```

日志示例：

```cpp
#include "Logger.hpp"
int main() {
    LOG_INFO << "程序启动";
    LOG_DEBUG << "调试信息";
    LOG_ERROR << "发生错误";
    return 0;
}
```

## 贡献指南

欢迎贡献代码、文档和建议！

1. Fork 本仓库，创建新分支
2. 提交你的修改，建议遵循 C++ 代码风格和注释规范
3. 提交 Pull Request，描述你的改动和用途
4. 如有问题或建议，请在 Issues 区留言

建议贡献内容包括：
- 新功能实现
- 跨平台兼容性增强
- 性能优化
- 文档完善
- 测试用例补充

贡献流程建议：

1. Fork 仓库并创建分支：`feature/描述` 或 `fix/描述`
2. 本地编译并运行现有测试（如有），确保不破坏现有功能
3. 提交清晰的 commit（每次修改目的明确）
4. 发起 Pull Request，提供改动说明与影响范围
5. 响应 Review 建议并更新 PR

开发者提示：
- 保持 API 向后兼容，若需破坏性更改请在 PR 描述中说明迁移步骤
- 项目当前使用 `gettimeofday`（Linux）和 `std::chrono`（Windows）获取时间
- README 中示例与宏开关（如 `TIMESTAMP_TEST`、`LOGMESSAGE_TEST`）可用于快速调试

## 注意事项

- 时间计算基于 Unix 时间戳（1970年1月1日00:00:00 UTC）
- Linux 使用 gettimeofday，Windows 使用 std::chrono
- 日志输出可自定义，默认输出到控制台
- .gitignore 已忽略 build/ 和 bin/ 目录

---
## 许可证

本项目采用 MIT 许可证，详情请参阅 LICENSE 文件。
