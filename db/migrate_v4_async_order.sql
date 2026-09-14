USE hyperticket;

SET @col := (SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='reservations' AND COLUMN_NAME='request_id');
SET @sql := IF(@col=0,
  'ALTER TABLE reservations ADD COLUMN request_id VARCHAR(64) DEFAULT NULL AFTER order_no',
  'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @idx := (SELECT COUNT(*) FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='reservations' AND INDEX_NAME='uq_resv_request_id');
SET @sql := IF(@idx=0,
  'CREATE UNIQUE INDEX uq_resv_request_id ON reservations(request_id)',
  'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
