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
  INDEX idx_users_username (username)
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
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CHECK (available_seats >= 0),
  INDEX idx_tickets_date (event_date),
  INDEX idx_tickets_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Reservations / bookings: one record per user reservation
CREATE TABLE IF NOT EXISTS reservations (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id INT NOT NULL,
  ticket_id INT NOT NULL,
  quantity INT NOT NULL DEFAULT 1,
  status ENUM('PENDING','CONFIRMED','CANCELLED','EXPIRED') NOT NULL DEFAULT 'CONFIRMED',
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CONSTRAINT fk_reservations_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT fk_reservations_ticket FOREIGN KEY (ticket_id) REFERENCES tickets(id) ON DELETE RESTRICT ON UPDATE CASCADE,
  INDEX idx_reservations_user (user_id),
  INDEX idx_reservations_ticket (ticket_id),
  INDEX idx_reservations_status (status)
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
