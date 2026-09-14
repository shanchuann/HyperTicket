-- HyperTicket v2 迁移：大麦网式功能扩展
-- 幂等设计：可重复执行（IF NOT EXISTS / 判断列存在）

USE hyperticket;

-- ── tickets 扩展：城市、详情、购票须知、艺人 ──
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

CALL add_col_if_missing('tickets', 'city',        "city VARCHAR(32) NOT NULL DEFAULT '北京'");
CALL add_col_if_missing('tickets', 'description', "description TEXT DEFAULT NULL");
CALL add_col_if_missing('tickets', 'notice',      "notice TEXT DEFAULT NULL");
CALL add_col_if_missing('tickets', 'artist',      "artist VARCHAR(128) DEFAULT NULL");

-- ── reservations 扩展：待支付过期时间 ──
CALL add_col_if_missing('reservations', 'expire_at', "expire_at DATETIME DEFAULT NULL");

DROP PROCEDURE IF EXISTS add_col_if_missing;

-- 索引（重复创建报错可忽略，这里用判断避免）
SET @idx := (SELECT COUNT(*) FROM information_schema.STATISTICS
             WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='tickets' AND INDEX_NAME='idx_tickets_city');
SET @s := IF(@idx=0, 'CREATE INDEX idx_tickets_city ON tickets(city)', 'SELECT 1');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @idx := (SELECT COUNT(*) FROM information_schema.STATISTICS
             WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='reservations' AND INDEX_NAME='idx_resv_pending_expire');
SET @s := IF(@idx=0, 'CREATE INDEX idx_resv_pending_expire ON reservations(status, expire_at)', 'SELECT 1');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- ── 收藏表 ──
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
