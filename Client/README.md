# Client 客户端

HyperTicket 的命令行客户端，面向终端用户，提供注册、登录、票务查询、预定、查看与取消预定。

## 技术栈

- **网络**：原生 BSD socket（`socket`/`connect`/`send`/`recv`），与服务端以换行分隔的 JSON 行通信。
- **序列化**：jsoncpp。
- 跨平台：Linux 与 Windows（条件编译，Windows 用 Winsock）。
- 核心类：`socket_client`，默认连接 `127.0.0.1:7000`。

## 功能与流程

| 方法 | 说明 |
|------|------|
| `Connect_server()` | 建立 TCP 连接 |
| `login()` | 输入手机号 + 密码登录，成功后保存返回的 `token` |
| `register_()` | 注册（手机号 + 密码 + 用户名） |
| `view()` | 拉取并展示在售票务 |
| `order()` | 选择票务下单（携带 token） |
| `view_my()` | 查看本人预定（携带 token） |
| `cancel()` | 取消指定预定（携带 token） |
| `exit_()` | 退出并关闭连接 |

## 会话 token

登录成功后，客户端从响应中保存 `token` 并在后续 ORDER / VIEW_MY / CANCEL 请求中携带。
服务端凭 token 识别用户身份，客户端无需也不应再上报手机号来标识自己。token 仅存于内存，重启客户端需重新登录。

## 输入校验

- 手机号：必须为 11 位。
- 密码复杂度由服务端强制（6–16 位），登录/注册时校验。

## 请求字段

发送的 JSON 关键字段：`type`（见 [Server 请求类型](../Server/README.md)）、`usertel`、`passward`、`username`、`token`、`index`。

## 构建与运行

```bash
# 在仓库根目录
cmake -S . -B build && cmake --build build -j
./bin/client
```

如需连接非默认地址，使用带参构造 `socket_client(ip, port)`。
