-- HyperTicket v11: establish an explicit schema compatibility gate.
-- Apply only after migrations v2-v10. Safe to run repeatedly.

USE hyperticket;

DROP PROCEDURE IF EXISTS verify_v11_prerequisites;
DELIMITER //
CREATE PROCEDURE verify_v11_prerequisites()
BEGIN
  IF NOT EXISTS (SELECT 1 FROM information_schema.TABLES
                 WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='events')
     OR NOT EXISTS (SELECT 1 FROM information_schema.TABLES
                    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments')
     OR NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
                    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='refunds'
                      AND COLUMN_NAME='max_attempts')
     OR NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
                    WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='reservations'
                      AND COLUMN_NAME='deleted_at') THEN
    SIGNAL SQLSTATE '45000'
      SET MESSAGE_TEXT='HyperTicket v11 requires migrations v2-v10 first';
  END IF;
END //
DELIMITER ;

CALL verify_v11_prerequisites();
DROP PROCEDURE IF EXISTS verify_v11_prerequisites;

DROP PROCEDURE IF EXISTS add_v11_action_tokens;
DELIMITER //
CREATE PROCEDURE add_v11_action_tokens()
BEGIN
  IF NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
                 WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments'
                   AND COLUMN_NAME='action_token') THEN
    ALTER TABLE payments ADD COLUMN action_token CHAR(32) NOT NULL DEFAULT '' AFTER status;
  END IF;
  IF NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
                 WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='refunds'
                   AND COLUMN_NAME='action_token') THEN
    ALTER TABLE refunds ADD COLUMN action_token CHAR(32) NOT NULL DEFAULT '' AFTER max_attempts;
  END IF;
END //
DELIMITER ;

CALL add_v11_action_tokens();
DROP PROCEDURE IF EXISTS add_v11_action_tokens;

CREATE TABLE IF NOT EXISTS schema_migrations (
  version INT PRIMARY KEY,
  name VARCHAR(128) NOT NULL,
  applied_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO schema_migrations(version,name) VALUES
  (1,'initial_schema'),(2,'ticket_features'),(3,'payment'),
  (4,'async_order'),(5,'auth_security'),(6,'verification'),
  (7,'payment_domain'),(8,'backend_reliability'),(9,'esports_category'),
  (10,'event_catalog'),(11,'schema_version_gate_and_action_leases')
ON DUPLICATE KEY UPDATE name=VALUES(name);
