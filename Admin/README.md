# Admin 管理端

面向运营人员的命令行管理工具，**直连 MySQL**（不经服务端、不经连接池），用于票务与用户管理。

## 技术栈

- 直接使用 MySQL C API，进程内维持一条持久连接（`MYSQL mysql;`）。
- 数据库配置经 [Common/AppConfig](../Common/README.md) 从 `config.json` / `.env` / 环境变量读取（`db_ip`/`db_user`/`db_passwd`/`db_name`），**不再硬编码密码**。
- 核心类：`AdminManager`，菜单驱动循环。

## 功能（`ADMIN_OP`）

| 值 | 操作 | 说明 |
|----|------|------|
| 1 | ADD_TICKET | 录入票务（场馆、座位数、日期），`INSERT` 到 tickets |
| 2 | VIEW_TICKETS | 查看全部票务 |
| 3 | VIEW_USERS | 查看全部用户（含状态：正常 / 黑名单） |
| 4 | VIEW_BLACKLIST | 查看黑名单用户（`status = 0`） |
| 5 | ADD_TO_BLACKLIST | 拉黑：`UPDATE users SET status = 0 WHERE tel = ?` |
| 6 | REMOVE_FROM_BLACKLIST | 解封：`UPDATE users SET status = 1 ...` |
| 7 | CANCEL_TICKET | 下架票务（软删除：`status = 0`） |
| 8 | EXIT | 退出 |

被拉黑（`status != 1`）的用户在服务端登录时会被拒绝（`BLACKLISTED`）。

## 使用

```bash
# 在仓库根目录
cmake -S . -B build && cmake --build build -j
./bin/admin     # 读取 config.json 中的 db.* 连接数据库
```

## 注意事项

- 管理端 SQL 目前以字符串拼接 + `mysql_query` 执行，**未使用预处理语句**（与服务端不同）。它是受信运营人员在本地使用的内部工具，请勿暴露给不可信输入；如需对外，应改造为参数化查询。
- 管理端不做并发控制，假定单人操作。

## 源码

- `include/admin.hpp`：`ADMIN_OP`、`AdminManager` 声明
- `src/admin.cpp`：连接、菜单与各管理操作实现
