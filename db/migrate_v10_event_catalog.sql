USE hyperticket;

CREATE TABLE IF NOT EXISTS venues (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(160) NOT NULL,
  venue_type ENUM('VENUE','CINEMA') NOT NULL DEFAULT 'VENUE',
  city VARCHAR(32) NOT NULL,
  address VARCHAR(255) NOT NULL,
  longitude DECIMAL(10,7) NOT NULL,
  latitude DECIMAL(10,7) NOT NULL,
  status TINYINT NOT NULL DEFAULT 1,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_venue_name_city (name, city),
  INDEX idx_venue_city_status (city, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS halls (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  venue_id BIGINT NOT NULL,
  name VARCHAR(96) NOT NULL,
  hall_format VARCHAR(64) NOT NULL DEFAULT '标准',
  seat_mode ENUM('RESERVED','GENERAL','MIXED') NOT NULL DEFAULT 'RESERVED',
  capacity INT NOT NULL DEFAULT 0,
  seat_template_json JSON DEFAULT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_hall_venue_name (venue_id, name),
  CONSTRAINT fk_hall_venue FOREIGN KEY (venue_id) REFERENCES venues(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS events (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  title VARCHAR(255) NOT NULL,
  category ENUM('movie','concert','performance','comedy','exhibition','esports','sports') NOT NULL,
  subtitle VARCHAR(255) NOT NULL DEFAULT '',
  organizer VARCHAR(160) NOT NULL DEFAULT '',
  artist VARCHAR(160) NOT NULL DEFAULT '',
  description TEXT,
  notice TEXT,
  cover_path VARCHAR(512) NOT NULL DEFAULT '',
  cover_source_url VARCHAR(1024) NOT NULL DEFAULT '',
  cover_author VARCHAR(160) NOT NULL DEFAULT '',
  city VARCHAR(32) NOT NULL,
  real_name_required TINYINT(1) NOT NULL DEFAULT 0,
  status ENUM('DRAFT','PUBLISHED','HIDDEN','CANCELLED') NOT NULL DEFAULT 'PUBLISHED',
  home_section ENUM('RECOMMENDED','MUST_SEE','COMING_SOON','NOW_SHOWING','HOT_SPORTS','CITY_PICKS') NOT NULL DEFAULT 'RECOMMENDED',
  ranking_weight INT NOT NULL DEFAULT 0,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  INDEX idx_event_discovery (status, home_section, ranking_weight),
  INDEX idx_event_category_city (category, city, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS event_sessions (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  event_id BIGINT NOT NULL,
  venue_id BIGINT NOT NULL,
  hall_id BIGINT DEFAULT NULL,
  legacy_ticket_id INT DEFAULT NULL,
  session_name VARCHAR(96) NOT NULL DEFAULT '',
  sale_start_at DATETIME(3) NOT NULL,
  sale_end_at DATETIME(3) NOT NULL,
  starts_at DATETIME(3) NOT NULL,
  ends_at DATETIME(3) DEFAULT NULL,
  timezone VARCHAR(64) NOT NULL DEFAULT 'Asia/Shanghai',
  status ENUM('SCHEDULED','ON_SALE','SOLD_OUT','ENDED','CANCELLED') NOT NULL DEFAULT 'SCHEDULED',
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_session_legacy_ticket (legacy_ticket_id),
  INDEX idx_session_event_time (event_id, starts_at),
  INDEX idx_session_sale_window (sale_start_at, sale_end_at, starts_at),
  CONSTRAINT fk_session_event FOREIGN KEY (event_id) REFERENCES events(id) ON DELETE CASCADE,
  CONSTRAINT fk_session_venue FOREIGN KEY (venue_id) REFERENCES venues(id) ON DELETE RESTRICT,
  CONSTRAINT fk_session_hall FOREIGN KEY (hall_id) REFERENCES halls(id) ON DELETE SET NULL,
  CONSTRAINT fk_session_ticket FOREIGN KEY (legacy_ticket_id) REFERENCES tickets(id) ON DELETE SET NULL,
  CHECK (sale_start_at < sale_end_at),
  CHECK (sale_end_at < starts_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ticket_tiers (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  session_id BIGINT NOT NULL,
  name VARCHAR(96) NOT NULL,
  price_minor BIGINT NOT NULL,
  currency CHAR(3) NOT NULL DEFAULT 'CNY',
  inventory INT NOT NULL,
  available_inventory INT NOT NULL,
  purchase_limit INT NOT NULL DEFAULT 6,
  seat_mode ENUM('RESERVED','GENERAL') NOT NULL DEFAULT 'RESERVED',
  sort_order INT NOT NULL DEFAULT 0,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_tier_session_name (session_id, name),
  CONSTRAINT fk_tier_session FOREIGN KEY (session_id) REFERENCES event_sessions(id) ON DELETE CASCADE,
  CHECK (available_inventory >= 0 AND available_inventory <= inventory)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS user_profiles (
  user_id INT PRIMARY KEY,
  display_name VARCHAR(64) NOT NULL DEFAULT '',
  avatar_path VARCHAR(512) NOT NULL DEFAULT '/media/avatars/default.webp',
  gender ENUM('UNSPECIFIED','MALE','FEMALE') NOT NULL DEFAULT 'UNSPECIFIED',
  birthday DATE DEFAULT NULL,
  city VARCHAR(32) NOT NULL DEFAULT '',
  bio VARCHAR(255) NOT NULL DEFAULT '',
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  CONSTRAINT fk_profile_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS attendees (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id INT NOT NULL,
  name VARCHAR(64) NOT NULL,
  id_type ENUM('PRC_ID','PASSPORT','HK_MACAO_PERMIT','TAIWAN_PERMIT') NOT NULL,
  id_number_ciphertext TEXT NOT NULL,
  id_number_hash CHAR(64) NOT NULL,
  id_number_masked VARCHAR(64) NOT NULL,
  phone_masked VARCHAR(32) NOT NULL DEFAULT '',
  is_default TINYINT(1) NOT NULL DEFAULT 0,
  deleted_at DATETIME(3) DEFAULT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_attendee_user_identity (user_id, id_number_hash),
  INDEX idx_attendee_user_active (user_id, deleted_at),
  CONSTRAINT fk_attendee_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS reservation_attendees (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  reservation_id BIGINT NOT NULL,
  session_id BIGINT DEFAULT NULL,
  seat_id BIGINT DEFAULT NULL,
  attendee_id BIGINT DEFAULT NULL,
  attendee_name VARCHAR(64) NOT NULL,
  id_type VARCHAR(32) NOT NULL,
  id_number_masked VARCHAR(64) NOT NULL,
  id_number_hash CHAR(64) NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_session_identity (session_id, id_number_hash),
  UNIQUE KEY uq_reservation_seat_attendee (reservation_id, seat_id),
  CONSTRAINT fk_ra_reservation FOREIGN KEY (reservation_id) REFERENCES reservations(id) ON DELETE CASCADE,
  CONSTRAINT fk_ra_session FOREIGN KEY (session_id) REFERENCES event_sessions(id) ON DELETE SET NULL,
  CONSTRAINT fk_ra_seat FOREIGN KEY (seat_id) REFERENCES seats(id) ON DELETE SET NULL,
  CONSTRAINT fk_ra_attendee FOREIGN KEY (attendee_id) REFERENCES attendees(id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS browsing_history (
  user_id INT NOT NULL,
  event_id BIGINT NOT NULL,
  view_count INT NOT NULL DEFAULT 1,
  last_viewed_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  PRIMARY KEY (user_id, event_id),
  INDEX idx_history_user_time (user_id, last_viewed_at),
  CONSTRAINT fk_history_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
  CONSTRAINT fk_history_event FOREIGN KEY (event_id) REFERENCES events(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS sale_reminders (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id INT NOT NULL,
  event_id BIGINT NOT NULL,
  session_id BIGINT DEFAULT NULL,
  email VARCHAR(255) NOT NULL,
  remind_at DATETIME(3) NOT NULL,
  status ENUM('PENDING','PROCESSING','SENT','FAILED','CANCELLED') NOT NULL DEFAULT 'PENDING',
  attempt_count INT NOT NULL DEFAULT 0,
  max_attempts INT NOT NULL DEFAULT 5,
  next_attempt_at DATETIME(3) NOT NULL,
  sent_at DATETIME(3) DEFAULT NULL,
  last_error VARCHAR(512) NOT NULL DEFAULT '',
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_reminder_user_event (user_id, event_id),
  INDEX idx_reminder_due (status, next_attempt_at),
  CONSTRAINT fk_reminder_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
  CONSTRAINT fk_reminder_event FOREIGN KEY (event_id) REFERENCES events(id) ON DELETE CASCADE,
  CONSTRAINT fk_reminder_session FOREIGN KEY (session_id) REFERENCES event_sessions(id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS sale_reminder_audit (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  reminder_id BIGINT NOT NULL,
  event VARCHAR(32) NOT NULL,
  detail VARCHAR(512) NOT NULL DEFAULT '',
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  INDEX idx_reminder_audit (reminder_id, created_at),
  CONSTRAINT fk_reminder_audit_reminder FOREIGN KEY (reminder_id) REFERENCES sale_reminders(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS event_favorites (
  user_id INT NOT NULL,
  event_id BIGINT NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  PRIMARY KEY (user_id, event_id),
  CONSTRAINT fk_event_favorite_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
  CONSTRAINT fk_event_favorite_event FOREIGN KEY (event_id) REFERENCES events(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
