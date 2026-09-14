# Redis Streams 异步下单架构

## 请求流程

1. `ORDER (type=5)` 完成会话和参数校验。
2. Redis Lua 原子检查库存、执行 `DECRBY`、写入 Stream，并初始化请求状态。
3. API 立即返回 `order_status=QUEUED` 和 `request_id`。
4. 消费者组异步执行 MySQL 事务、库存条件更新及订单写入。
5. `ORDER_QUERY (type=25)` 返回 `QUEUED`、`PENDING` 或 `FAILED`。

客户端可提供 8-64 位且仅含字母数字、`_`、`-` 的 `request_id`。重试同一请求
必须复用该 ID；Redis 状态键和 MySQL `reservations.request_id` 唯一索引共同保证幂等。

## 一致性与恢复

- Redis Lua 将扣库存、入队和状态初始化放在一次原子操作中。
- MySQL 仍做条件库存更新，库存不能写成负数。
- DB 提交后、ACK 前崩溃时由 `XAUTOCLAIM` 恢复，唯一请求 ID 防止重复订单。
- 终态失败通过 Lua 原子执行库存补偿、死信写入、状态更新和 ACK。
- DB 暂时不可用时消息留在 Pending 列表，超过投递次数后进入
  `hyperticket:orders:dead` 并补偿库存。
- 活跃票库存 key 不使用自然 TTL，避免尚未落库的预扣减因过期丢失。

## 协议

```json
{"type":5,"token":"...","index":10,"quantity":2,"request_id":"checkout_123"}
```

成功接收返回 `QUEUED`。随后发送：

```json
{"type":25,"token":"...","request_id":"checkout_123"}
```

成功落库返回 `PENDING` 和 `reservation_id`；失败返回 `FAILED` 和 `reason`。

## 部署与监控

现有数据库先执行 `db/migrate_v4_async_order.sql`；新安装使用 `db/init.sql`。
多个实例必须使用不同 `consumer_name`，并共享 stream 和 consumer group。

启用 `metrics.enabled=true` 后访问 `http://127.0.0.1:8080/metrics`。新增指标：

- `hyperticket_order_queue_published_total`
- `hyperticket_order_queue_consumed_total`
- `hyperticket_order_queue_failed_total`
- `hyperticket_order_queue_pending`
- `hyperticket_inventory_sold_out_total`
- `hyperticket_inventory_compensation_total`
- `hyperticket_order_queue_consume_duration_seconds`（标准 Histogram）
