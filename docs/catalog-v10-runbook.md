# Catalog v10 运行手册

本文说明当前活动目录的迁移、演示数据重建和验证流程。脚本会清空业务域数据，不应用于未经确认的生产数据库。

## 前置检查

1. Windows MySQL 已运行，WSL2 能通过 `.env` 中的地址连接。
2. 已从 `config.example.json` 和 `.env.example` 创建本地配置。
3. 用户 `13008569663` 已存在，作为订单、收藏、浏览历史和提醒演示账号。
4. 已确认本次操作允许清除现有业务数据。

## 应用领域模型

确认 Windows MySQL 可访问后，在 WSL2 中运行迁移与种子脚本：

```bash
./scripts/migrate-and-seed-v10.sh
```

脚本会在清理业务表前，将现有业务数据压缩备份到 `backups/business-<timestamp>.sql.gz`。它保留用户、管理员、手机号/邮箱验证状态、Session 和认证安全配置；如果用户 `13008569663` 不存在，种子过程会主动终止。

种子数据创建 35 个拟真虚构活动和 70 个场次，覆盖电影、演唱会、演出、脱口秀、展览、电竞赛事和体育赛事，并包含场馆经纬度、厅馆、票档、座位、浏览历史、收藏、提醒以及多种订单/支付状态。封面位于 `clients/user-client/public/media/events`，来源说明见 `clients/user-client/public/media/ATTRIBUTION.md`。

执行结束应输出 `events=35` 和 `sessions=70`。随后可做只读校验：

```bash
mysql --protocol=tcp -h 127.0.0.1 -P 3306 -u root -p hyperticket \
  -e "SELECT category, COUNT(*) FROM events GROUP BY category ORDER BY category;"
```

需要恢复迁移前业务数据时，先停服并确认目标库，再解压对应备份。恢复会影响当前业务数据，必须由操作者单独执行，不由迁移脚本自动回滚。

## 启动服务栈

```bash
./bin/ser
cd websocket-bridge && npm start
cd clients/user-client && npm run dev
cd clients/admin-client && npm run dev -- --port 5174
```

C++ 服务继续使用 Windows MySQL 和 WSL2 Redis。公开目录请求遇到过期的可选浏览器 token 时会按游客继续；收藏、提醒、订单和个人中心等认证操作仍严格拒绝无效 Session。

## 管理端流程

管理端提供活动、场馆、厅馆、场次和票档标签页，以及开售提醒审计表。创建场次时校验 `sale_start_at < sale_end_at < starts_at`，同时创建兼容票品记录与独立座位库存，并支持复制场次配置。

## 回归检查

```bash
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./scripts/run-payment-e2e.sh
```

```powershell
cd clients/user-client; npm run build
cd ../admin-client; npm run build
```

最后检查目录的 7 个分类、场次可售数量、多座选择、模拟支付、订单/电子票、个人中心，以及管理员场次时间校验和提醒审计。
