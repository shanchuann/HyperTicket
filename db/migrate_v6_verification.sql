USE hyperticket;

SET @add_email_verified = IF(
  EXISTS(SELECT 1 FROM information_schema.columns
         WHERE table_schema=DATABASE() AND table_name='users' AND column_name='email_verified_at'),
  'SELECT 1',
  'ALTER TABLE users ADD COLUMN email_verified_at DATETIME(3) DEFAULT NULL'
);
PREPARE stmt FROM @add_email_verified; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @add_phone_verified = IF(
  EXISTS(SELECT 1 FROM information_schema.columns
         WHERE table_schema=DATABASE() AND table_name='users' AND column_name='phone_verified_at'),
  'SELECT 1',
  'ALTER TABLE users ADD COLUMN phone_verified_at DATETIME(3) DEFAULT NULL'
);
PREPARE stmt FROM @add_phone_verified; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @add_email_unique = IF(
  EXISTS(SELECT 1 FROM information_schema.statistics
         WHERE table_schema=DATABASE() AND table_name='users' AND index_name='uq_users_email'),
  'SELECT 1',
  'CREATE UNIQUE INDEX uq_users_email ON users(email)'
);
PREPARE stmt FROM @add_email_unique; EXECUTE stmt; DEALLOCATE PREPARE stmt;

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
