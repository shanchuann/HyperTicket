-- HyperTicket database initialization and migration script
-- Creates schema and tables with constraints, indexes, and example transaction

CREATE DATABASE IF NOT EXISTS hyperticket
  CHARACTER SET = utf8mb4
  COLLATE = utf8mb4_unicode_ci;
USE hyperticket;

-- Users table: store credentials as hashes + optional salt, timestamps
CREATE TABLE IF NOT EXISTS users (
  id INT AUTO_INCREMENT PRIMARY KEY,
  tel CHAR(11) NOT NULL,
  username VARCHAR(64) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  salt VARCHAR(64) DEFAULT NULL,
  email VARCHAR(255) DEFAULT NULL,
  email_verified_at DATETIME(3) DEFAULT NULL,
  phone_verified_at DATETIME(3) DEFAULT NULL,
  status TINYINT NOT NULL DEFAULT 1, -- 0=disabled,1=active,2=locked
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  last_login DATETIME DEFAULT NULL,
  UNIQUE KEY uq_users_tel (tel),
  UNIQUE KEY uq_users_email (email),
  INDEX idx_users_username (username),
  INDEX idx_users_tel_status (tel, status),  -- 复合索引：登录时同时检查手机号和状态
  INDEX idx_users_status (status)            -- 管理端查询黑名单用户
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Tickets: master data for ticketed events
CREATE TABLE IF NOT EXISTS tickets (
  id INT AUTO_INCREMENT PRIMARY KEY,
  title VARCHAR(255) NOT NULL,
  venue VARCHAR(255) DEFAULT NULL,
  total_seats INT NOT NULL DEFAULT 0,
  available_seats INT NOT NULL DEFAULT 0,
  event_date DATE NOT NULL,
  status TINYINT NOT NULL DEFAULT 1, -- 0=hidden/cancelled,1=open,2=soldout
  category VARCHAR(32) DEFAULT 'concert', -- concert/sports/movie/theater/exhibition
  price INT NOT NULL DEFAULT 0,           -- base price (yuan)
  city VARCHAR(32) NOT NULL DEFAULT '北京',  -- 演出城市（筛选用）
  description TEXT DEFAULT NULL,            -- 演出详情简介
  notice TEXT DEFAULT NULL,                 -- 购票须知
  artist VARCHAR(128) DEFAULT NULL,         -- 艺人/团体
  cover_image MEDIUMTEXT DEFAULT NULL, -- base64 encoded cover image (optional)
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CHECK (available_seats >= 0),
  INDEX idx_tickets_date (event_date),
  INDEX idx_tickets_status (status),
  INDEX idx_tickets_category (category),
  INDEX idx_tickets_city (city),
  INDEX idx_tickets_status_date (status, event_date),
  INDEX idx_tickets_list_covering (status, id, title, venue, total_seats, available_seats, event_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Seats: one row per seat per ticket event, auto-generated on ticket creation
CREATE TABLE IF NOT EXISTS seats (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  ticket_id INT NOT NULL,
  seat_label VARCHAR(20) NOT NULL,  -- e.g. A1, B12
  row_label VARCHAR(8) NOT NULL,
  col_num SMALLINT NOT NULL,
  tier ENUM('VIP','Standard','Economy') NOT NULL DEFAULT 'Standard',
  price INT NOT NULL DEFAULT 0,     -- per-seat price (yuan), may differ by tier
  status ENUM('AVAILABLE','SOLD') NOT NULL DEFAULT 'AVAILABLE',
  reservation_id BIGINT NULL,
  UNIQUE KEY uq_seat (ticket_id, seat_label),
  INDEX idx_seats_ticket_status (ticket_id, status),
  CONSTRAINT fk_seats_ticket FOREIGN KEY (ticket_id) REFERENCES tickets(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Reservations / bookings: one record per user reservation
CREATE TABLE IF NOT EXISTS reservations (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id INT NOT NULL,
  ticket_id INT NOT NULL,
  quantity INT NOT NULL DEFAULT 1,
  status ENUM('PENDING','CONFIRMED','CANCELLED','EXPIRED') NOT NULL DEFAULT 'CONFIRMED',
  expire_at DATETIME DEFAULT NULL, -- PENDING 订单支付截止时间，超时定时任务回收
  order_no VARCHAR(32) DEFAULT NULL, -- 真实订单号 HT{YYYYMMDD}{ID:06d}，下单提交后生成
  request_id VARCHAR(64) DEFAULT NULL, -- 异步下单幂等 ID
  deleted_at DATETIME(3) DEFAULT NULL, -- 用户侧逻辑删除，保留审计链
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY uq_resv_order_no (order_no),
  UNIQUE KEY uq_resv_request_id (request_id),
  CONSTRAINT fk_reservations_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT fk_reservations_ticket FOREIGN KEY (ticket_id) REFERENCES tickets(id) ON DELETE RESTRICT ON UPDATE CASCADE,
  INDEX idx_reservations_user (user_id),
  INDEX idx_reservations_ticket (ticket_id),
  INDEX idx_reservations_status (status),
  INDEX idx_reservations_user_status (user_id, status),  -- 复合索引：查询用户的有效订单
  INDEX idx_reservations_ticket_status (ticket_id, status),  -- 复合索引：统计票务预订情况
  INDEX idx_reservations_created (created_at),  -- 按时间查询订单（报表、清理过期订单）
  INDEX idx_reservations_user_visible (user_id, deleted_at, id),
  INDEX idx_resv_pending_expire (status, expire_at)  -- 定时回收超时 PENDING 订单
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Payments: 支付领域模型（金额使用最小货币单位；CNY 为分）
CREATE TABLE IF NOT EXISTS payments (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  payment_no VARCHAR(32) DEFAULT NULL,
  reservation_id BIGINT NOT NULL,
  user_id INT NOT NULL,
  amount_minor BIGINT NOT NULL,
  currency CHAR(3) NOT NULL DEFAULT 'CNY',
  provider VARCHAR(16) NOT NULL,
  provider_transaction_id VARCHAR(128) DEFAULT NULL,
  client_idempotency_key VARCHAR(64) NOT NULL,
  status ENUM('CREATED','PROCESSING','SUCCEEDED','FAILED','CLOSED','REFUNDING','PARTIALLY_REFUNDED','REFUNDED') NOT NULL DEFAULT 'CREATED',
  action_token CHAR(32) NOT NULL DEFAULT '',
  next_action_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_pay_no (payment_no),
  UNIQUE KEY uq_pay_user_idempotency (user_id, client_idempotency_key),
  INDEX idx_pay_resv_status (reservation_id, status),
  INDEX idx_pay_action (status, next_action_at),
  INDEX idx_pay_provider_txn (provider, provider_transaction_id),
  CONSTRAINT fk_pay_resv FOREIGN KEY (reservation_id) REFERENCES reservations(id) ON DELETE CASCADE,
  CONSTRAINT fk_pay_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS payment_events (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  payment_id BIGINT NOT NULL,
  from_status VARCHAR(32) NOT NULL DEFAULT '',
  to_status VARCHAR(32) NOT NULL,
  source VARCHAR(32) NOT NULL,
  detail VARCHAR(512) NOT NULL DEFAULT '',
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  INDEX idx_payment_events_payment (payment_id, created_at),
  CONSTRAINT fk_payment_events_payment FOREIGN KEY (payment_id) REFERENCES payments(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS refunds (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  payment_id BIGINT NOT NULL,
  refund_no VARCHAR(32) NOT NULL,
  amount_minor BIGINT NOT NULL,
  currency CHAR(3) NOT NULL DEFAULT 'CNY',
  status ENUM('CREATED','PROCESSING','SUCCEEDED','FAILED') NOT NULL DEFAULT 'CREATED',
  provider_refund_id VARCHAR(128) DEFAULT NULL,
  reason VARCHAR(255) NOT NULL DEFAULT '',
  failure_reason VARCHAR(255) NOT NULL DEFAULT '',
  attempt_count INT NOT NULL DEFAULT 0,
  max_attempts INT NOT NULL DEFAULT 5,
  action_token CHAR(32) NOT NULL DEFAULT '',
  next_action_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_refund_no (refund_no),
  INDEX idx_refund_payment (payment_id),
  INDEX idx_refund_action (status, next_action_at),
  CONSTRAINT fk_refund_payment FOREIGN KEY (payment_id) REFERENCES payments(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS payment_webhook_events (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  provider VARCHAR(16) NOT NULL,
  provider_event_id VARCHAR(128) NOT NULL,
  payment_id BIGINT DEFAULT NULL,
  signature_valid TINYINT(1) NOT NULL DEFAULT 0,
  payload_hash CHAR(64) NOT NULL,
  processing_status ENUM('RECEIVED','PROCESSED','REJECTED') NOT NULL DEFAULT 'RECEIVED',
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  processed_at DATETIME(3) DEFAULT NULL,
  UNIQUE KEY uq_webhook_provider_event (provider, provider_event_id),
  INDEX idx_webhook_payment (payment_id),
  CONSTRAINT fk_webhook_payment FOREIGN KEY (payment_id) REFERENCES payments(id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Optional admin table for manager accounts
CREATE TABLE IF NOT EXISTS admins (
  id INT AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(64) NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL,
  role VARCHAR(64) DEFAULT 'operator',
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  last_login DATETIME DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 默认管理员（用户名: admin，密码: password，bcrypt 哈希）
INSERT IGNORE INTO admins (username, password_hash, role) VALUES
  ('admin', '$2b$12$VAyaS3YlHX5emcU6Zf01duPbDdvwfB4uQ8HPuwUr8q3I82XwuouMS', 'superadmin');

-- Shared authentication throttles: account/IP/device counters survive restarts
-- and are enforced consistently by multiple backend instances.
CREATE TABLE IF NOT EXISTS auth_throttles (
  scope VARCHAR(32) NOT NULL,
  subject VARCHAR(255) NOT NULL,
  failure_count INT NOT NULL DEFAULT 0,
  window_started_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  locked_until DATETIME(3) DEFAULT NULL,
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (scope, subject),
  INDEX idx_auth_throttle_locked (locked_until)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS security_audit (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  actor_type VARCHAR(16) NOT NULL,
  actor VARCHAR(255) NOT NULL,
  event VARCHAR(64) NOT NULL,
  ip_address VARCHAR(64) NOT NULL DEFAULT '',
  detail VARCHAR(512) NOT NULL DEFAULT '',
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  INDEX idx_security_actor_time (actor_type, actor, created_at),
  INDEX idx_security_event_time (event, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS auth_challenges (
  id CHAR(64) PRIMARY KEY,
  user_id INT DEFAULT NULL,
  subject VARCHAR(255) NOT NULL,
  destination VARCHAR(255) NOT NULL,
  purpose VARCHAR(32) NOT NULL,
  channel VARCHAR(16) NOT NULL,
  code_hash VARCHAR(255) NOT NULL,
  attempts INT NOT NULL DEFAULT 0,
  max_attempts INT NOT NULL DEFAULT 5,
  expires_at DATETIME(3) NOT NULL,
  verified_at DATETIME(3) DEFAULT NULL,
  grant_hash VARCHAR(255) DEFAULT NULL,
  grant_expires_at DATETIME(3) DEFAULT NULL,
  consumed_at DATETIME(3) DEFAULT NULL,
  requested_ip VARCHAR(64) NOT NULL DEFAULT '',
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  INDEX idx_challenge_destination_time (destination, purpose, channel, created_at),
  INDEX idx_challenge_expiry (expires_at),
  INDEX idx_challenge_user (user_id, purpose)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Simple audit log for important actions (reservation changes)
CREATE TABLE IF NOT EXISTS reservation_audit (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  reservation_id BIGINT NOT NULL,
  action VARCHAR(64) NOT NULL,
  detail TEXT,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_audit_reservation (reservation_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 用户收藏（想看）
CREATE TABLE IF NOT EXISTS favorites (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id INT NOT NULL,
  ticket_id INT NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uq_fav (user_id, ticket_id),
  INDEX idx_fav_user (user_id),
  CONSTRAINT fk_fav_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
  CONSTRAINT fk_fav_ticket FOREIGN KEY (ticket_id) REFERENCES tickets(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Example safe booking transaction (to run from application code)
-- Use SELECT ... FOR UPDATE on the tickets row to lock inventory before decrementing.
-- PSEUDOCODE SQL (illustrative):
-- START TRANSACTION;
-- SELECT available_seats FROM tickets WHERE id = ? FOR UPDATE;
-- IF available_seats >= :qty THEN
--   UPDATE tickets SET available_seats = available_seats - :qty WHERE id = ?;
--   INSERT INTO reservations (user_id, ticket_id, quantity, status) VALUES (..., 'CONFIRMED');
--   COMMIT;
-- ELSE
--   ROLLBACK; -- insufficient inventory
-- END IF;

-- Note: create application-level retry/backoff on transient deadlocks.

-- ============================================================
-- 性能优化说明
-- ============================================================
-- 1. 复合索引优化：
--    - idx_users_tel_status: 登录时同时检查手机号和状态，避免两次索引查找
--    - idx_tickets_status_date: 查询在售票务并按日期排序，单次索引扫描
--    - idx_reservations_user_status: 查询用户有效订单，避免全表扫描
--
-- 2. 覆盖索引优化：
--    - idx_tickets_list_covering: 包含票务列表查询的所有字段，避免回表
--    - 适用于 SELECT id,title,venue,total_seats,available_seats,event_date FROM tickets WHERE status=1
--
-- 3. 索引使用建议：
--    - 高频查询：用户登录、票务列表、我的订单 - 已优化
--    - 写入性能：索引会略微降低INSERT/UPDATE性能，但查询收益远大于写入成本
--    - 索引维护：定期运行 ANALYZE TABLE 更新统计信息
--
-- 4. 监控建议：
--    - 使用 EXPLAIN 分析慢查询
--    - 监控 slow_query_log（>100ms的查询）
--    - 定期检查索引使用率：SELECT * FROM sys.schema_unused_indexes;
