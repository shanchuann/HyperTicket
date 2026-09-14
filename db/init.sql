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
  status TINYINT NOT NULL DEFAULT 1, -- 0=disabled,1=active,2=locked
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  last_login DATETIME DEFAULT NULL,
  UNIQUE KEY uq_users_tel (tel),
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
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY uq_resv_order_no (order_no),
  CONSTRAINT fk_reservations_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT fk_reservations_ticket FOREIGN KEY (ticket_id) REFERENCES tickets(id) ON DELETE RESTRICT ON UPDATE CASCADE,
  INDEX idx_reservations_user (user_id),
  INDEX idx_reservations_ticket (ticket_id),
  INDEX idx_reservations_status (status),
  INDEX idx_reservations_user_status (user_id, status),  -- 复合索引：查询用户的有效订单
  INDEX idx_reservations_ticket_status (ticket_id, status),  -- 复合索引：统计票务预订情况
  INDEX idx_reservations_created (created_at),  -- 按时间查询订单（报表、清理过期订单）
  INDEX idx_resv_pending_expire (status, expire_at)  -- 定时回收超时 PENDING 订单
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Payments: 支付流水表（v3 支付模块）
-- 发起支付即写入一条 PROCESSING 流水（已提交模拟网关、等待异步结算）；
-- 定时任务到 settle_at 后结算为 SUCCESS/FAILED；
-- 结算成功但订单已失效（超时回收/取消）时补偿为 REFUNDED。
CREATE TABLE IF NOT EXISTS payments (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  payment_no VARCHAR(32) DEFAULT NULL,        -- 支付单号 PY{YYYYMMDD}{ID:08d}，创建后生成
  reservation_id BIGINT NOT NULL,
  user_id INT NOT NULL,
  amount INT NOT NULL DEFAULT 0,              -- 应付金额（元），创建时按选座价/票面价快照
  method VARCHAR(16) NOT NULL DEFAULT 'MOCK', -- 支付渠道：MOCK/ALIPAY/WECHAT
  status ENUM('PROCESSING','SUCCESS','FAILED','REFUNDED') NOT NULL DEFAULT 'PROCESSING',
  settle_at DATETIME(3) NOT NULL,             -- 模拟网关结算时间，到点由定时任务结算
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY uq_pay_no (payment_no),
  INDEX idx_pay_resv_status (reservation_id, status),
  INDEX idx_pay_settle (status, settle_at),   -- 定时结算扫描
  CONSTRAINT fk_pay_resv FOREIGN KEY (reservation_id) REFERENCES reservations(id) ON DELETE CASCADE,
  CONSTRAINT fk_pay_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Optional admin table for manager accounts
CREATE TABLE IF NOT EXISTS admins (
  id INT AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(64) NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL,
  role VARCHAR(64) DEFAULT 'operator',
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 默认管理员（用户名: admin，密码: password，bcrypt 哈希）
INSERT IGNORE INTO admins (username, password_hash, role) VALUES
  ('admin', '$2b$12$/0yFcwnUmEoY9pkiH.dI5OUrkzielDdM5/gUih9IUweMQDBnmLG7G', 'superadmin');

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
