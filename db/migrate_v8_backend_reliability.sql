-- HyperTicket v8: preserve order audit history and make refunds retryable.
-- Idempotent and safe to run repeatedly after v7.

USE hyperticket;

DROP PROCEDURE IF EXISTS reliability_add_col;
DELIMITER //
CREATE PROCEDURE reliability_add_col(
  IN table_name_value VARCHAR(64), IN column_name_value VARCHAR(64), IN column_ddl TEXT)
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME=table_name_value
      AND COLUMN_NAME=column_name_value
  ) THEN
    SET @ddl=CONCAT('ALTER TABLE ',table_name_value,' ADD COLUMN ',column_ddl);
    PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;
  END IF;
END //
DELIMITER ;

CALL reliability_add_col('reservations','deleted_at','deleted_at DATETIME(3) DEFAULT NULL');
CALL reliability_add_col('refunds','attempt_count','attempt_count INT NOT NULL DEFAULT 0');
CALL reliability_add_col('refunds','max_attempts','max_attempts INT NOT NULL DEFAULT 5');
DROP PROCEDURE IF EXISTS reliability_add_col;

SET @idx=(SELECT COUNT(*) FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='reservations'
    AND INDEX_NAME='idx_reservations_user_visible');
SET @ddl=IF(@idx=0,
  'CREATE INDEX idx_reservations_user_visible ON reservations(user_id,deleted_at,id)',
  'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- Retry old failed refunds unless they already exhausted the new retry budget.
UPDATE refunds
SET status='PROCESSING',next_action_at=NOW(3)
WHERE status='FAILED' AND attempt_count<max_attempts;
