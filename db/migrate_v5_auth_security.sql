USE hyperticket;

SET @add_last_login = IF(
  EXISTS(SELECT 1 FROM information_schema.columns
         WHERE table_schema = DATABASE() AND table_name = 'admins' AND column_name = 'last_login'),
  'SELECT 1',
  'ALTER TABLE admins ADD COLUMN last_login DATETIME DEFAULT NULL'
);
PREPARE stmt FROM @add_last_login;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @add_updated_at = IF(
  EXISTS(SELECT 1 FROM information_schema.columns
         WHERE table_schema = DATABASE() AND table_name = 'admins' AND column_name = 'updated_at'),
  'SELECT 1',
  'ALTER TABLE admins ADD COLUMN updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP'
);
PREPARE stmt FROM @add_updated_at;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

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
