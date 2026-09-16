USE hyperticket;

DELIMITER $$
DROP PROCEDURE IF EXISTS seed_v10_catalog$$
CREATE PROCEDURE seed_v10_catalog()
BEGIN
  DECLARE demo_user_id INT DEFAULT NULL;
  DECLARE done INT DEFAULT 0;
  DECLARE event_id BIGINT;
  DECLARE event_title VARCHAR(255);
  DECLARE event_category VARCHAR(32);
  DECLARE event_city VARCHAR(32);
  DECLARE event_artist VARCHAR(160);
  DECLARE event_cover VARCHAR(512);
  DECLARE event_real_name TINYINT;
  DECLARE venue_id BIGINT;
  DECLARE hall_id BIGINT;
  DECLARE ticket_id BIGINT;
  DECLARE session_id BIGINT;
  DECLARE session_index INT;
  DECLARE seat_index INT;
  DECLARE base_price INT;
  DECLARE starts_at DATETIME(3);
  DECLARE demo_ticket_1 BIGINT;
  DECLARE demo_ticket_2 BIGINT;
  DECLARE demo_ticket_3 BIGINT;
  DECLARE demo_ticket_4 BIGINT;
  DECLARE demo_order_1 BIGINT;
  DECLARE demo_order_2 BIGINT;
  DECLARE event_cursor CURSOR FOR
    SELECT e.id,e.title,e.category,e.city,e.artist,e.cover_path,e.real_name_required,
           v.id,h.id,CASE e.category WHEN 'movie' THEN 6800 WHEN 'exhibition' THEN 8800
             WHEN 'comedy' THEN 18000 WHEN 'performance' THEN 28000 WHEN 'esports' THEN 32000
             WHEN 'sports' THEN 26000 ELSE 48000 END
    FROM events e JOIN venues v ON v.city=e.city
    JOIN halls h ON h.venue_id=v.id
    WHERE v.id=(SELECT MIN(v2.id) FROM venues v2 WHERE v2.city=e.city)
      AND h.id=(SELECT MIN(h2.id) FROM halls h2 WHERE h2.venue_id=v.id)
    ORDER BY e.id;
  DECLARE CONTINUE HANDLER FOR NOT FOUND SET done=1;

  SELECT id INTO demo_user_id FROM users WHERE tel='13008569663' LIMIT 1;
  IF demo_user_id IS NULL THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Demo user 13008569663 does not exist; seed aborted';
  END IF;

  SET FOREIGN_KEY_CHECKS=0;
  DELETE FROM payment_webhook_events;
  DELETE FROM payment_events;
  DELETE FROM refunds;
  DELETE FROM payments;
  DELETE FROM reservation_attendees;
  DELETE FROM reservation_audit;
  DELETE FROM seats;
  DELETE FROM reservations;
  DELETE FROM favorites;
  DELETE FROM event_favorites;
  DELETE FROM browsing_history;
  DELETE FROM sale_reminder_audit;
  DELETE FROM sale_reminders;
  DELETE FROM ticket_tiers;
  DELETE FROM event_sessions;
  DELETE FROM events;
  DELETE FROM halls;
  DELETE FROM venues;
  DELETE FROM tickets;
  DELETE FROM attendees;
  SET FOREIGN_KEY_CHECKS=1;

  INSERT INTO venues(name,venue_type,city,address,longitude,latitude) VALUES
    ('北京国家体育馆','VENUE','北京','北京市朝阳区天辰东路9号',116.3908040,39.9993450),
    ('北京星幕影城国贸店','CINEMA','北京','北京市朝阳区建国路93号',116.4672100,39.9094800),
    ('上海梅赛德斯奔驰文化中心','VENUE','上海','上海市浦东新区世博大道1200号',121.4931000,31.1907400),
    ('上海光陆影城徐家汇店','CINEMA','上海','上海市徐汇区肇嘉浜路1111号',121.4369100,31.1943700),
    ('广州天河体育中心','VENUE','广州','广州市天河区天河路299号',113.3213500,23.1407000),
    ('广州云幕影城珠江新城店','CINEMA','广州','广州市天河区花城大道85号',113.3226500,23.1187200),
    ('深圳湾体育中心','VENUE','深圳','深圳市南山区滨海大道3001号',113.9499100,22.5267300),
    ('深圳南山文体中心','VENUE','深圳','深圳市南山区南山大道2106号',113.9237200,22.5335200),
    ('成都凤凰山体育公园','VENUE','成都','成都市金牛区北星大道一段4228号',104.0747800,30.7714400),
    ('成都天府艺术公园','VENUE','成都','成都市金牛区金牛大道金牛坝路',104.0246100,30.7102000),
    ('杭州奥体中心体育场','VENUE','杭州','杭州市滨江区飞虹路3号',120.2390200,30.2294400),
    ('杭州运河文化艺术中心','VENUE','杭州','杭州市拱墅区运河湾',120.1488200,30.3191600),
    ('武汉光谷国际网球中心','VENUE','武汉','武汉市江夏区佛祖岭一路2号',114.4831000,30.4484200),
    ('武汉琴台大剧院','VENUE','武汉','武汉市汉阳区知音大道7号',114.2528800,30.5586400);

  INSERT INTO halls(venue_id,name,hall_format,seat_mode,capacity)
    SELECT id,IF(venue_type='CINEMA','1号激光厅','主馆'),IF(venue_type='CINEMA','杜比全景声','综合场馆'),'RESERVED',IF(venue_type='CINEMA',120,160) FROM venues;
  INSERT INTO halls(venue_id,name,hall_format,seat_mode,capacity)
    SELECT id,IF(venue_type='CINEMA','IMAX厅','副馆'),IF(venue_type='CINEMA','IMAX激光','黑匣子'),'RESERVED',IF(venue_type='CINEMA',96,100) FROM venues;

  INSERT INTO events(title,category,subtitle,organizer,artist,description,notice,cover_path,cover_source_url,cover_author,city,real_name_required,status,home_section,ranking_weight) VALUES
    ('星际回声','movie','太空深处的返航讯号','远光影业','林遥、陈屿','一支深空救援队追踪失联信号，发现时间并未按原路流动。','电影票一经出票按影院规则退改。','/media/events/movie-1.webp','https://unsplash.com','Unsplash','北京',0,'PUBLISHED','NOW_SHOWING',92),
    ('雾港来信','movie','海边小城的三封旧信','南岸影业','周宁、许舟','修复师在旧胶片中找到跨越二十年的未寄信件。','请于开场前取票入场。','/media/events/movie-2.webp','https://pexels.com','Pexels','上海',0,'PUBLISHED','MUST_SEE',88),
    ('逆风航线','movie','高空救援动作片','云层工作室','宋野、顾川','极端天气中的民航机组完成一次不可能的备降。','特殊影厅请按场次说明入场。','/media/events/movie-3.webp','https://unsplash.com','Unsplash','广州',0,'PUBLISHED','COMING_SOON',80),
    ('纸上月光','movie','城市爱情轻喜剧','拾光影业','沈沐、叶澄','两个替陌生人写信的人在同一座城市反复错过。','儿童需凭票入场。','/media/events/movie-4.webp','https://pexels.com','Pexels','深圳',0,'PUBLISHED','RECOMMENDED',76),
    ('山海食记','movie','风味人文纪录电影','原野纪录片厂','纪录片','沿海到高原，记录七个家庭的一餐一饭。','纪录片含方言字幕。','/media/events/movie-5.webp','https://unsplash.com','Unsplash','成都',0,'PUBLISHED','CITY_PICKS',72),
    ('折光乐队「返场之前」巡演','concert','十年精选现场','北纬声场','折光乐队','以全编制舞台重访十年代表作品。','实名入场，证件须与观演人一致。','/media/events/concert-1.webp','https://unsplash.com','Unsplash','北京',1,'PUBLISHED','MUST_SEE',98),
    ('鹿鸣「沿途有风」音乐会','concert','城市民谣专场','回声文化','鹿鸣','木吉他与弦乐编制的秋日专场。','请勿携带专业摄影设备。','/media/events/concert-2.webp','https://pexels.com','Pexels','上海',1,'PUBLISHED','NOW_SHOWING',91),
    ('NEON SEA 霓虹海电子现场','concert','沉浸式电子音乐现场','脉冲现场','NEON SEA','多屏视觉与空间声场共同构成的电子音乐夜。','演出全程站席区域请注意安全。','/media/events/concert-3.webp','https://unsplash.com','Unsplash','广州',1,'PUBLISHED','RECOMMENDED',87),
    ('夏末邮局合唱团专场','concert','把没寄出的歌唱完','纸飞机文化','夏末邮局合唱团','合唱、独唱与观众共创段落组成的温暖现场。','一人一票，凭实名信息入场。','/media/events/concert-4.webp','https://pexels.com','Pexels','杭州',1,'PUBLISHED','COMING_SOON',83),
    ('蓝鲸室内乐团城市音乐会','concert','从古典到电影配乐','蓝鲸艺术','蓝鲸室内乐团','精选古典乐章与原创电影配乐。','迟到观众需听从工作人员安排。','/media/events/concert-5.webp','https://unsplash.com','Unsplash','武汉',1,'PUBLISHED','CITY_PICKS',79),
    ('话剧《第七码头》','performance','一夜之间的六个选择','镜面剧社','镜面剧社','暴雨封港之夜，六名陌生人在候船室交换秘密。','演出时长约130分钟。','/media/events/performance-1.webp','https://pexels.com','Pexels','上海',0,'PUBLISHED','MUST_SEE',94),
    ('舞剧《青铜之光》','performance','古蜀文明当代表达','蜀风舞团','蜀风舞团','以现代舞语汇重构青铜文明的想象。','演出开始后暂停入场。','/media/events/performance-2.webp','https://unsplash.com','Unsplash','成都',0,'PUBLISHED','RECOMMENDED',90),
    ('音乐剧《零点列车》','performance','开往明天的最后一班车','开往剧场','开往剧场','七位乘客在午夜列车上重新选择人生目的地。','演出含中场休息。','/media/events/performance-3.webp','https://pexels.com','Pexels','北京',0,'PUBLISHED','NOW_SHOWING',86),
    ('亲子剧《云朵修理铺》','performance','适合5岁以上儿童','小小剧场','小小剧场','修理匠帮助失去颜色的云朵找回雨水。','儿童均需购票。','/media/events/performance-4.webp','https://unsplash.com','Unsplash','深圳',0,'PUBLISHED','CITY_PICKS',74),
    ('昆曲新编《游园一梦》','performance','传统声腔新舞台','水磨剧场','水磨剧场','保留水磨腔韵味，探索当代舞台空间。','请提前30分钟入场。','/media/events/performance-5.webp','https://pexels.com','Pexels','杭州',0,'PUBLISHED','COMING_SOON',82),
    ('周末不加班脱口秀专场','comedy','打工人的下班现场','笑点工厂','阿迟、可乐','围绕通勤、会议和生活的小剧场拼盘。','演出内容不建议12岁以下观看。','/media/events/comedy-1.webp','https://unsplash.com','Unsplash','北京',0,'PUBLISHED','NOW_SHOWING',89),
    ('林墨单口喜剧《半熟》','comedy','人生没有标准答案','开放麦文化','林墨','关于成长、家庭与城市生活的个人专场。','请勿录音录像。','/media/events/comedy-2.webp','https://pexels.com','Pexels','上海',0,'PUBLISHED','MUST_SEE',85),
    ('粤语喜剧夜《讲真》','comedy','普通话字幕场','南方笑场','南方笑场演员组','用粤语观察城市生活，现场提供字幕屏。','部分内容使用粤语。','/media/events/comedy-3.webp','https://unsplash.com','Unsplash','广州',0,'PUBLISHED','CITY_PICKS',81),
    ('即兴喜剧《今晚听你的》','comedy','观众决定故事走向','即刻剧团','即刻剧团','每一场都由观众关键词生成全新故事。','互动环节自愿参与。','/media/events/comedy-4.webp','https://pexels.com','Pexels','武汉',0,'PUBLISHED','RECOMMENDED',77),
    ('喜剧拼盘《城市漫游》','comedy','六位演员轮番上场','笑浪俱乐部','笑浪演员组','来自不同城市的生活观察主题拼盘。','座位先到按区入座。','/media/events/comedy-5.webp','https://unsplash.com','Unsplash','成都',0,'PUBLISHED','COMING_SOON',70),
    ('未来栖居：城市想象展','exhibition','建筑与数字艺术跨界展','界面美术馆','联合策展','从可持续材料到交互城市模型的综合展览。','开放票在有效期内单次核销。','/media/events/exhibition-1.webp','https://pexels.com','Pexels','上海',0,'PUBLISHED','MUST_SEE',93),
    ('深海一万米沉浸展','exhibition','进入无光带','蓝境科学中心','蓝境科学中心','用声光装置呈现深海生态与科学探索。','部分区域光线较暗。','/media/events/exhibition-2.webp','https://unsplash.com','Unsplash','深圳',0,'PUBLISHED','NOW_SHOWING',88),
    ('纸的千种形态设计展','exhibition','材料实验与手工档案','天府设计馆','联合策展','聚焦纸张工艺、出版与空间装置。','展厅禁止饮食。','/media/events/exhibition-3.webp','https://pexels.com','Pexels','成都',0,'PUBLISHED','CITY_PICKS',78),
    ('像素考古：早期电子游戏展','exhibition','可玩的数字档案','光栅博物馆','光栅博物馆','从街机到掌机的互动式游戏史展览。','互动设备请依次体验。','/media/events/exhibition-4.webp','https://unsplash.com','Unsplash','杭州',0,'PUBLISHED','RECOMMENDED',84),
    ('风从江城来摄影展','exhibition','城市与人的瞬间','江城影像馆','青年摄影师群展','以街道、江面和日常人物组成的城市肖像。','允许非商业摄影。','/media/events/exhibition-5.webp','https://pexels.com','Pexels','武汉',0,'PUBLISHED','COMING_SOON',73),
    ('星环冠军杯总决赛','esports','五局三胜巅峰对决','星环电竞联盟','赤曜 vs 白塔','年度积分前两名争夺星环冠军。','实名制入场，禁止代票。','/media/events/esports-1.webp','https://unsplash.com','Unsplash','上海',1,'PUBLISHED','HOT_SPORTS',99),
    ('峡谷城市邀请赛','esports','八城战队淘汰赛','城市电竞协会','八城代表队','城市战队单败淘汰赛与明星表演赛。','请提前60分钟完成安检。','/media/events/esports-2.webp','https://pexels.com','Pexels','深圳',1,'PUBLISHED','HOT_SPORTS',95),
    ('极点战术竞技公开赛','esports','线下总决赛','极点赛事','十二支晋级队伍','多轮积分决出年度公开赛冠军。','场馆内禁止使用闪光灯。','/media/events/esports-3.webp','https://unsplash.com','Unsplash','成都',1,'PUBLISHED','MUST_SEE',90),
    ('银河竞速大师赛','esports','模拟竞速职业赛','银河竞速联盟','职业车手阵容','高拟真赛车模拟器上的城市街道赛。','观众可体验赛前模拟器。','/media/events/esports-4.webp','https://pexels.com','Pexels','广州',1,'PUBLISHED','COMING_SOON',86),
    ('方块创想建造赛','esports','创意与速度双赛道','创想社区','社区明星选手','创意建造与限时挑战组成的轻竞技赛事。','适合亲子观赛。','/media/events/esports-5.webp','https://unsplash.com','Unsplash','杭州',1,'PUBLISHED','CITY_PICKS',75),
    ('华东猎隼 vs 江城航线','sports','城市篮球联赛焦点战','城市篮球联盟','华东猎隼、江城航线','季后赛席位争夺战。','实名制入场，对号入座。','/media/events/sports-1.webp','https://pexels.com','Pexels','上海',1,'PUBLISHED','HOT_SPORTS',97),
    ('深圳浪潮 vs 广州南星','sports','湾区足球德比','湾区足球联盟','深圳浪潮、广州南星','湾区城市足球联赛德比战。','禁止携带瓶装饮料。','/media/events/sports-2.webp','https://unsplash.com','Unsplash','深圳',1,'PUBLISHED','HOT_SPORTS',94),
    ('成都山脊网球公开赛决赛','sports','男女单打决赛日','山脊网球协会','决赛选手待定','公开赛冠军日双场连看。','比赛时间可能受天气影响。','/media/events/sports-3.webp','https://pexels.com','Pexels','成都',1,'PUBLISHED','MUST_SEE',89),
    ('钱塘夜跑城市接力赛','sports','四人团队接力','城市跑步会','大众赛事','沿江夜景赛道的城市接力活动。','参赛票需完成健康声明。','/media/events/sports-4.webp','https://unsplash.com','Unsplash','杭州',1,'PUBLISHED','CITY_PICKS',82),
    ('武汉飞羽羽毛球团体赛','sports','俱乐部年度决赛','飞羽体育','四强俱乐部','五场制俱乐部团体决赛。','观众席禁止使用闪光灯。','/media/events/sports-5.webp','https://pexels.com','Pexels','武汉',1,'PUBLISHED','RECOMMENDED',80);

  OPEN event_cursor;
  event_loop: LOOP
    FETCH event_cursor INTO event_id,event_title,event_category,event_city,event_artist,event_cover,event_real_name,venue_id,hall_id,base_price;
    IF done=1 THEN LEAVE event_loop; END IF;
    SET session_index=0;
    WHILE session_index<2 DO
      SET starts_at=DATE_ADD(DATE_ADD(NOW(),INTERVAL
        (CASE WHEN event_id<=7 THEN -10+session_index*2 WHEN event_id<=14 THEN 2+session_index*2 ELSE 12+MOD(event_id,20)+session_index*9 END) DAY),
        INTERVAL (18+MOD(event_id,3)) HOUR);
      INSERT INTO tickets(title,venue,total_seats,available_seats,event_date,status,cover_image,category,price,city,artist,description,notice)
        SELECT event_title,v.name,80,80,DATE(starts_at),1,event_cover,event_category,FLOOR(base_price/100),event_city,event_artist,e.description,e.notice
        FROM venues v JOIN events e ON e.id=event_id WHERE v.id=venue_id;
      SET ticket_id=LAST_INSERT_ID();
      INSERT INTO event_sessions(event_id,venue_id,hall_id,legacy_ticket_id,session_name,sale_start_at,sale_end_at,starts_at,ends_at,status)
        VALUES(event_id,venue_id,hall_id,ticket_id,CONCAT('第',session_index+1,'场'),
          CASE WHEN event_id<=14 THEN DATE_SUB(NOW(),INTERVAL 5 DAY) ELSE DATE_ADD(NOW(),INTERVAL 2 DAY) END,
          DATE_SUB(starts_at,INTERVAL 1 DAY),starts_at,DATE_ADD(starts_at,INTERVAL 2 HOUR),
          CASE WHEN starts_at<NOW() THEN 'ENDED' WHEN DATE_ADD(starts_at,INTERVAL -1 DAY)<=NOW() THEN 'ON_SALE' ELSE 'SCHEDULED' END);
      SET session_id=LAST_INSERT_ID();
      INSERT INTO ticket_tiers(session_id,name,price_minor,inventory,available_inventory,purchase_limit,seat_mode,sort_order) VALUES
        (session_id,'臻享区',base_price*2,16,16,4,'RESERVED',1),
        (session_id,'优选区',base_price,32,32,6,'RESERVED',2),
        (session_id,'标准区',FLOOR(base_price*0.7),32,32,6,'RESERVED',3);
      SET seat_index=1;
      WHILE seat_index<=80 DO
        INSERT INTO seats(ticket_id,seat_label,row_label,col_num,tier,price)
        VALUES(ticket_id,CONCAT(CHAR(64+CEIL(seat_index/10)),MOD(seat_index-1,10)+1),CHAR(64+CEIL(seat_index/10)),MOD(seat_index-1,10)+1,
          IF(seat_index<=20,'VIP',IF(seat_index<=50,'Standard','Economy')),
          IF(seat_index<=20,FLOOR(base_price*2/100),IF(seat_index<=50,FLOOR(base_price/100),FLOOR(base_price*0.7/100))));
        SET seat_index=seat_index+1;
      END WHILE;
      SET session_index=session_index+1;
    END WHILE;
  END LOOP;
  CLOSE event_cursor;

  INSERT INTO user_profiles(user_id,display_name,avatar_path,city,bio)
    VALUES(demo_user_id,'演示观众','/media/avatars/default.webp','北京','喜欢现场，也喜欢在电影散场后走一段路。')
    ON DUPLICATE KEY UPDATE display_name=VALUES(display_name),avatar_path=VALUES(avatar_path),city=VALUES(city),bio=VALUES(bio);
  INSERT INTO attendees(user_id,name,id_type,id_number_ciphertext,id_number_hash,id_number_masked,phone_masked,is_default) VALUES
    (demo_user_id,'林舟','PRC_ID','seed-encrypted-demo-1',SHA2('110101199201011234',256),'110***********1234','130****9663',1),
    (demo_user_id,'许澄','PASSPORT','seed-encrypted-demo-2',SHA2('E12345678',256),'E12**5678','138****2048',0);
  INSERT INTO event_favorites(user_id,event_id) SELECT demo_user_id,id FROM events ORDER BY ranking_weight DESC LIMIT 5;
  INSERT INTO browsing_history(user_id,event_id,view_count,last_viewed_at) SELECT demo_user_id,id,MOD(id,5)+1,DATE_SUB(NOW(),INTERVAL MOD(id,8) DAY) FROM events ORDER BY id LIMIT 5;
  INSERT INTO sale_reminders(user_id,event_id,session_id,email,remind_at,status,next_attempt_at)
    SELECT demo_user_id,e.id,MIN(s.id),u.email,MIN(s.sale_start_at),'PENDING',MIN(s.sale_start_at)
    FROM events e JOIN event_sessions s ON s.event_id=e.id JOIN users u ON u.id=demo_user_id
    WHERE u.email_verified_at IS NOT NULL GROUP BY e.id,u.email ORDER BY e.ranking_weight DESC LIMIT 3;

  SELECT legacy_ticket_id INTO demo_ticket_1 FROM event_sessions ORDER BY id LIMIT 1;
  SELECT legacy_ticket_id INTO demo_ticket_2 FROM event_sessions ORDER BY id LIMIT 1 OFFSET 2;
  SELECT legacy_ticket_id INTO demo_ticket_3 FROM event_sessions ORDER BY id LIMIT 1 OFFSET 4;
  SELECT legacy_ticket_id INTO demo_ticket_4 FROM event_sessions ORDER BY id LIMIT 1 OFFSET 6;
  INSERT INTO reservations(user_id,ticket_id,quantity,status,expire_at,order_no,request_id,created_at) VALUES
    (demo_user_id,demo_ticket_1,2,'CONFIRMED',NULL,CONCAT('HTDEMO',DATE_FORMAT(NOW(),'%m%d'),'01'),'demo-v10-confirmed-1',DATE_SUB(NOW(),INTERVAL 2 DAY));
  SET demo_order_1=LAST_INSERT_ID();
  INSERT INTO reservations(user_id,ticket_id,quantity,status,expire_at,order_no,request_id,created_at) VALUES
    (demo_user_id,demo_ticket_2,1,'CONFIRMED',NULL,CONCAT('HTDEMO',DATE_FORMAT(NOW(),'%m%d'),'02'),'demo-v10-confirmed-2',DATE_SUB(NOW(),INTERVAL 1 DAY));
  SET demo_order_2=LAST_INSERT_ID();
  INSERT INTO reservations(user_id,ticket_id,quantity,status,expire_at,order_no,request_id,created_at) VALUES
    (demo_user_id,demo_ticket_3,1,'PENDING',DATE_ADD(NOW(),INTERVAL 15 MINUTE),CONCAT('HTDEMO',DATE_FORMAT(NOW(),'%m%d'),'03'),'demo-v10-pending',NOW()),
    (demo_user_id,demo_ticket_4,1,'CANCELLED',NULL,CONCAT('HTDEMO',DATE_FORMAT(NOW(),'%m%d'),'04'),'demo-v10-cancelled',DATE_SUB(NOW(),INTERVAL 5 DAY));
  UPDATE seats SET status='SOLD',reservation_id=demo_order_1 WHERE ticket_id=demo_ticket_1 ORDER BY id LIMIT 2;
  UPDATE seats SET status='SOLD',reservation_id=demo_order_2 WHERE ticket_id=demo_ticket_2 ORDER BY id LIMIT 1;
  UPDATE tickets SET available_seats=available_seats-2 WHERE id=demo_ticket_1;
  UPDATE tickets SET available_seats=available_seats-1 WHERE id=demo_ticket_2;
  INSERT INTO payments(payment_no,reservation_id,user_id,amount_minor,currency,provider,provider_transaction_id,client_idempotency_key,status,next_action_at) VALUES
    (CONCAT('PYDEMO',DATE_FORMAT(NOW(),'%m%d'),'01'),demo_order_1,demo_user_id,13600,'CNY','MOCK','MOCK-DEMO-01','demo-payment-1','SUCCEEDED',NOW()),
    (CONCAT('PYDEMO',DATE_FORMAT(NOW(),'%m%d'),'02'),demo_order_2,demo_user_id,6800,'CNY','MOCK','MOCK-DEMO-02','demo-payment-2','SUCCEEDED',NOW());
  INSERT INTO reservation_attendees(reservation_id,session_id,seat_id,attendee_id,attendee_name,id_type,id_number_masked,id_number_hash)
    SELECT demo_order_1,es.id,(SELECT MIN(id) FROM seats WHERE reservation_id=demo_order_1),a.id,a.name,a.id_type,a.id_number_masked,a.id_number_hash
    FROM event_sessions es JOIN attendees a ON a.user_id=demo_user_id AND a.is_default=1
    WHERE es.legacy_ticket_id=demo_ticket_1 LIMIT 1;
  INSERT INTO reservation_attendees(reservation_id,session_id,seat_id,attendee_id,attendee_name,id_type,id_number_masked,id_number_hash)
    SELECT demo_order_1,es.id,(SELECT MAX(id) FROM seats WHERE reservation_id=demo_order_1),a.id,a.name,a.id_type,a.id_number_masked,a.id_number_hash
    FROM event_sessions es JOIN attendees a ON a.user_id=demo_user_id AND a.is_default=0
    WHERE es.legacy_ticket_id=demo_ticket_1 LIMIT 1;
END$$
DELIMITER ;

CALL seed_v10_catalog();
DROP PROCEDURE seed_v10_catalog;
