# 自动发布与部署工作流

## 触发规则

向 `main` 推送后，CI 先执行后端构建与 CTest。CI 成功且最新提交标题以以下任一前缀开头时，`Release` 工作流才会运行发布任务：

```text
feat: ...
feat(scope): ...
fix: ...
fix(scope): ...
change: ...
```

其他提交（例如 `docs:`、`test:`、`chore:`）只运行 CI，不产生镜像或客户端 Release。匹配忽略大小写，并支持 Conventional Commits 的 scope 与 `!` 标记。

## 发布产物

每次发布使用 `v0.0.<workflow run number>` 作为不可变发布标签，并同时记录完整提交 SHA。

| 产物 | 地址/位置 | 标签 |
|---|---|---|
| Web 镜像 | `ghcr.io/shanchuann/hyperticket-web` | `latest`、发布标签、`sha-<12位>` |
| 后端镜像 | `ghcr.io/shanchuann/hyperticket-backend` | `latest`、发布标签、`sha-<12位>` |
| 用户客户端 | GitHub Release 附件 | Windows NSIS/MSI |
| 管理客户端 | GitHub Release 附件 | Windows NSIS/MSI |

Web 镜像提供：

- `/`：用户端。
- `/admin/`：管理端。
- `/ws`：WebSocket 到后端 TCP 的桥接。
- `/healthz`：容器健康检查。

Release 会先以草稿创建。两个客户端都完成构建和上传后才转为公开，避免用户下载到不完整版本。

## GHCR 权限

工作流使用仓库内置的 `GITHUB_TOKEN` 推送镜像，不需要单独保存 GitHub Token。首次发布后，在 GitHub Packages 中确认两个包的可见性：

- 公共仓库可以把包设为 Public，服务器即可匿名拉取。
- 私有包需在服务器预先执行 `docker login ghcr.io`，使用具有 `read:packages` 权限的细粒度 Token。

## 服务器首次准备

服务器需要 Git、Docker Engine 和 Docker Compose v2，并克隆本仓库：

```bash
git clone https://github.com/shanchuann/HyperTicket.git /opt/hyperticket
cd /opt/hyperticket
cp deploy/.env.production.example deploy/.env.production
chmod 600 deploy/.env.production
```

编辑 `deploy/.env.production`，至少设置现有 MySQL 的地址、账号与密码、随机网关令牌和 `WS_ALLOWED_ORIGINS`。发布流程不会自动执行数据库迁移或种子脚本；当前后端要求 v11，可用 `scripts/bootstrap-schema.sh` 初始化新库，或对 v10 库执行 `scripts/apply-schema-version-migration.sh`。

Linux 宿主机连接远程 Windows MySQL 时直接填写 Windows 可达 IP，不要使用 `127.0.0.1`。`host.docker.internal` 在 Docker Desktop 中可用，在普通 Linux Docker 上通常需要改为实际地址。

首次手动验证：

```bash
docker compose --env-file deploy/.env.production \
  -f deploy/compose.production.yml pull
docker compose --env-file deploy/.env.production \
  -f deploy/compose.production.yml up -d
docker compose --env-file deploy/.env.production \
  -f deploy/compose.production.yml ps
```

## 开启自动部署

在 GitHub 仓库创建名为 `production` 的 Environment，然后配置：

Repository variable（仓库级，不要只配置为 Environment variable）：

| 名称 | 值 |
|---|---|
| `AUTO_DEPLOY_ENABLED` | `true` |
| `PUBLIC_WS_URL` | 桌面安装包使用的公开 `wss://.../ws` 地址；为空会阻止发布 |

Environment secrets：

| 名称 | 说明 |
|---|---|
| `DEPLOY_HOST` | 服务器 IP 或域名 |
| `DEPLOY_PORT` | SSH 端口，例如 `22` |
| `DEPLOY_USER` | SSH 用户 |
| `DEPLOY_SSH_KEY` | 对应服务器公钥的私钥全文 |
| `DEPLOY_KNOWN_HOSTS` | `ssh-keyscan -H <host>` 的固定输出 |
| `DEPLOY_PATH` | 仓库绝对路径，例如 `/opt/hyperticket` |

`DEPLOY_KNOWN_HOSTS` 必须固定配置，工作流不会在运行时盲目信任服务器主机密钥。

开启后，工作流在镜像与客户端 Release 全部成功后执行：

```text
git pull --ff-only -> docker compose pull -> docker compose up -d
```

部署使用本次提交对应的 `sha-<12位>` 镜像，而不是浮动的 `latest`，确保服务器实际版本可追踪。部署失败不会回滚数据库，也不会删除旧镜像；可把 `IMAGE_TAG` 改为上一版本的 SHA 标签后手动重新执行 Compose 完成应用回滚。

## 安全边界

- 不要把数据库密码、SMTP 授权码或 SSH 私钥写入仓库。
- 服务端 TCP `7000` 只在 Compose 内网供 WebSocket Bridge 使用，不映射到公网。
- Web 公网入口应由 Nginx/Caddy 提供 HTTPS/WSS，再反向代理到 Web 容器 `8080`。
- `HYPERTICKET_GATEWAY_TOKEN` 必须为足够长的随机值，Bridge 与后端保持一致；`WS_ALLOWED_ORIGINS` 应包含 Web 域名和实际桌面 WebView Origin。
- 当前手机号验证码和支付均为模拟流程；不要配置或宣称真实短信、真实扣款能力。
- 自动部署账号仅授予目标目录和 Docker 所需的最小权限。
