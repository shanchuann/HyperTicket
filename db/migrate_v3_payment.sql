-- HyperTicket v3 迁移：支付模块（payments 支付流水表 + reservations.order_no 补列）
-- 幂等设计：可重复执行（IF NOT EXISTS / 判断列存在）

USE hyperticket;

-- ── reservations 补列：真实订单号（此前仅存在于线上库，补进版本化 schema）──
DROP PROCEDURE IF EXISTS add_col_if_missing;
DELIMITER //
CREATE PROCEDURE add_col_if_missing(
  IN tbl VARCHAR(64), IN col VARCHAR(64), IN ddl TEXT)
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = tbl AND COLUMN_NAME = col
  ) THEN
    SET @s = CONCAT('ALTER TABLE ', tbl, ' ADD COLUMN ', ddl);
    PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;
  END IF;
END //
DELIMITER ;

CALL add_col_if_missing('reservations', 'order_no', "order_no VARCHAR(32) DEFAULT NULL");

DROP PROCEDURE IF EXISTS add_col_if_missing;

SET @idx := (SELECT COUNT(*) FROM information_schema.STATISTICS
             WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='reservations' AND INDEX_NAME='uq_resv_order_no');
SET @s := IF(@idx=0, 'CREATE UNIQUE INDEX uq_resv_order_no ON reservations(order_no)', 'SELECT 1');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- ── 支付流水表（详见 init.sql 中的字段注释）──
CREATE TABLE IF NOT EXISTS payments (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  payment_no VARCHAR(32) DEFAULT NULL,
  reservation_id BIGINT NOT NULL,
  user_id INT NOT NULL,
  amount INT NOT NULL DEFAULT 0,
  method VARCHAR(16) NOT NULL DEFAULT 'MOCK',
  status ENUM('PROCESSING','SUCCESS','FAILED','REFUNDED') NOT NULL DEFAULT 'PROCESSING',
  settle_at DATETIME(3) NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY uq_pay_no (payment_no),
  INDEX idx_pay_resv_status (reservation_id, status),
  INDEX idx_pay_settle (status, settle_at),
  CONSTRAINT fk_pay_resv FOREIGN KEY (reservation_id) REFERENCES reservations(id) ON DELETE CASCADE,
  CONSTRAINT fk_pay_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
