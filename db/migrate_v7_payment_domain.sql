-- HyperTicket v7: payment domain model, cents, idempotency, events and refunds.
-- Safe to run repeatedly against a v3+ database.

USE hyperticket;

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

DROP PROCEDURE IF EXISTS payment_add_col;
DELIMITER //
CREATE PROCEDURE payment_add_col(IN col_name VARCHAR(64), IN col_ddl TEXT)
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments' AND COLUMN_NAME=col_name
  ) THEN
    SET @ddl=CONCAT('ALTER TABLE payments ADD COLUMN ',col_ddl);
    PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;
  END IF;
END //
DELIMITER ;

CALL payment_add_col('amount_minor', 'amount_minor BIGINT NULL');
CALL payment_add_col('currency', "currency CHAR(3) NULL DEFAULT 'CNY'");
CALL payment_add_col('provider', "provider VARCHAR(16) NULL DEFAULT 'MOCK'");
CALL payment_add_col('provider_transaction_id', 'provider_transaction_id VARCHAR(128) DEFAULT NULL');
CALL payment_add_col('client_idempotency_key', 'client_idempotency_key VARCHAR(64) NULL');
CALL payment_add_col('next_action_at', 'next_action_at DATETIME(3) NULL');
DROP PROCEDURE IF EXISTS payment_add_col;

-- Temporarily retain SUCCESS while old rows are converted.
ALTER TABLE payments MODIFY status
  ENUM('CREATED','PROCESSING','SUCCESS','SUCCEEDED','FAILED','CLOSED','REFUNDING','PARTIALLY_REFUNDED','REFUNDED')
  NOT NULL DEFAULT 'CREATED';

SET @has_amount=(SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments' AND COLUMN_NAME='amount');
SET @ddl=IF(@has_amount>0,
  'UPDATE payments SET amount_minor=amount*100 WHERE amount_minor IS NULL', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_method=(SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments' AND COLUMN_NAME='method');
SET @ddl=IF(@has_method>0,
  "UPDATE payments SET provider=method WHERE provider IS NULL OR provider=''", 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @has_settle=(SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments' AND COLUMN_NAME='settle_at');
SET @ddl=IF(@has_settle>0,
  'UPDATE payments SET next_action_at=settle_at WHERE next_action_at IS NULL', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

UPDATE payments SET status='SUCCEEDED' WHERE status='SUCCESS';
UPDATE payments SET amount_minor=0 WHERE amount_minor IS NULL;
UPDATE payments SET currency='CNY' WHERE currency IS NULL OR currency='';
UPDATE payments SET provider='MOCK' WHERE provider IS NULL OR provider='';
UPDATE payments SET client_idempotency_key=CONCAT('legacy-',id)
  WHERE client_idempotency_key IS NULL OR client_idempotency_key='';
UPDATE payments SET next_action_at=COALESCE(updated_at,created_at,NOW(3)) WHERE next_action_at IS NULL;

ALTER TABLE payments
  MODIFY amount_minor BIGINT NOT NULL,
  MODIFY currency CHAR(3) NOT NULL DEFAULT 'CNY',
  MODIFY provider VARCHAR(16) NOT NULL,
  MODIFY client_idempotency_key VARCHAR(64) NOT NULL,
  MODIFY next_action_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  MODIFY status ENUM('CREATED','PROCESSING','SUCCEEDED','FAILED','CLOSED','REFUNDING','PARTIALLY_REFUNDED','REFUNDED')
    NOT NULL DEFAULT 'CREATED';

SET @idx=(SELECT COUNT(*) FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments' AND INDEX_NAME='uq_pay_user_idempotency');
SET @ddl=IF(@idx=0,
  'CREATE UNIQUE INDEX uq_pay_user_idempotency ON payments(user_id,client_idempotency_key)', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @idx=(SELECT COUNT(*) FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments' AND INDEX_NAME='idx_pay_action');
SET @ddl=IF(@idx=0,
  'CREATE INDEX idx_pay_action ON payments(status,next_action_at)', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @idx=(SELECT COUNT(*) FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments' AND INDEX_NAME='idx_pay_provider_txn');
SET @ddl=IF(@idx=0,
  'CREATE INDEX idx_pay_provider_txn ON payments(provider,provider_transaction_id)', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

DROP PROCEDURE IF EXISTS payment_drop_legacy_col;
DELIMITER //
CREATE PROCEDURE payment_drop_legacy_col(IN col_name VARCHAR(64))
BEGIN
  IF EXISTS (
    SELECT 1 FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments' AND COLUMN_NAME=col_name
  ) THEN
    SET @ddl=CONCAT('ALTER TABLE payments DROP COLUMN ',col_name);
    PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;
  END IF;
END //
DELIMITER ;

SET @idx=(SELECT COUNT(*) FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments' AND INDEX_NAME='idx_pay_settle');
SET @ddl=IF(@idx>0, 'DROP INDEX idx_pay_settle ON payments', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;
CALL payment_drop_legacy_col('amount');
CALL payment_drop_legacy_col('method');
CALL payment_drop_legacy_col('settle_at');
DROP PROCEDURE IF EXISTS payment_drop_legacy_col;

CREATE TABLE IF NOT EXISTS payment_events (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  payment_id BIGINT NOT NULL,
  from_status VARCHAR(32) NOT NULL DEFAULT '',
  to_status VARCHAR(32) NOT NULL,
  source VARCHAR(32) NOT NULL,
  detail VARCHAR(512) NOT NULL DEFAULT '',
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  INDEX idx_payment_events_payment (payment_id,created_at),
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
  next_action_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_refund_no (refund_no),
  INDEX idx_refund_payment (payment_id),
  INDEX idx_refund_action (status,next_action_at),
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
  UNIQUE KEY uq_webhook_provider_event (provider,provider_event_id),
  INDEX idx_webhook_payment (payment_id),
  CONSTRAINT fk_webhook_payment FOREIGN KEY (payment_id) REFERENCES payments(id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
