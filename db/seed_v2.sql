-- HyperTicket v2 种子数据：多城市、多分类的真实感演出数据（参考大麦网品类）
-- 幂等：按 title 判重，可重复执行

USE hyperticket;

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '周杰伦「嘉年华」世界巡回演唱会-北京站' AS title, '国家体育场（鸟巢）' AS venue, '北京' AS city,
  2000 AS total_seats, 2000 AS available_seats, '2026-09-12' AS event_date, 1 AS status,
  'concert' AS category, 880 AS price, '周杰伦' AS artist,
  '华语流行天王周杰伦携「嘉年华」世界巡回演唱会震撼回归。本场演出将带来《七里香》《晴天》《稻香》等经典曲目全新编排，配合顶级舞美与视觉特效，打造沉浸式音乐盛宴。' AS description,
  '1. 本演出实行实名制购票，一张身份证限购4张。\n2. 演出票品为有价证券，一经售出概不退换。\n3. 1.2米以下儿童谢绝入场。\n4. 请提前90分钟入场安检。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '五月天「回到那一天」25周年巡回-上海站' AS title, '上海体育场' AS venue, '上海' AS city,
  1800 AS total_seats, 1800 AS available_seats, '2026-08-22' AS event_date, 1 AS status,
  'concert' AS category, 680 AS price, '五月天' AS artist,
  '五月天成军25周年纪念巡演，与百万歌迷重返青春现场。《倔强》《温柔》《突然好想你》，人生无限公司永不打烊。' AS description,
  '1. 实名制购票入场，人证票合一。\n2. 演出前30天可申请退票，收取10%手续费。\n3. 禁止携带专业摄录设备。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '林俊杰 JJ20 FINAL LAP 世界巡回演唱会-广州站' AS title, '广州天河体育中心' AS venue, '广州' AS city,
  1500 AS total_seats, 1500 AS available_seats, '2026-10-03' AS event_date, 1 AS status,
  'concert' AS category, 780 AS price, '林俊杰' AS artist,
  'JJ20 世界巡回演唱会最终章。二十年音乐旅程的告白与感谢，《江南》《修炼爱情》《不为谁而作的歌》与你共赴最终圈。' AS description,
  '1. 实名制购票，一单限购2张。\n2. 售出不退不换。\n3. 请勿携带饮料食品入场。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '2026 CBA总决赛 G4' AS title, '五棵松体育馆' AS venue, '北京' AS city,
  1200 AS total_seats, 1200 AS available_seats, '2026-08-05' AS event_date, 1 AS status,
  'sports' AS category, 380 AS price, NULL AS artist,
  'CBA总决赛巅峰对决第四战。冠军悬念一触即发，见证捧杯时刻。现场DJ、啦啦队表演、中场互动环节精彩不停。' AS description,
  '1. 凭票入场，对号入座。\n2. 比赛日期如有变动以官方公告为准。\n3. 禁止携带打火机、瓶装液体。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '2026赛季中超联赛 上海海港 vs 山东泰山' AS title, '浦东足球场' AS venue, '上海' AS city,
  2500 AS total_seats, 2500 AS available_seats, '2026-08-15' AS event_date, 1 AS status,
  'sports' AS category, 200 AS price, NULL AS artist,
  '中超争冠焦点战。海港主场迎战泰山，榜首之争一票难求。' AS description,
  '1. 球票实名制。\n2. 客队球迷请从北门入场。\n3. 禁止携带烟花爆竹等违禁品。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '开心麻花爆笑舞台剧《乌龙山伯爵》' AS title, '北京喜剧院' AS venue, '北京' AS city,
  600 AS total_seats, 600 AS available_seats, '2026-08-08' AS event_date, 1 AS status,
  'theater' AS category, 280 AS price, '开心麻花' AS artist,
  '开心麻花经典爆笑舞台剧，连续演出十年常演不衰。谢蟹浓缩了小人物的悲欢，笑中带泪的都市寓言。' AS description,
  '1. 演出时长约150分钟含中场休息。\n2. 迟到观众请在中场休息时入场。\n3. 一人一票，儿童全票。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '话剧《茶馆》——北京人民艺术剧院' AS title, '首都剧场' AS venue, '北京' AS city,
  400 AS total_seats, 400 AS available_seats, '2026-09-01' AS event_date, 1 AS status,
  'theater' AS category, 380 AS price, '北京人艺' AS artist,
  '老舍经典名作，北京人艺镇院之宝。一座茶馆，半部中国近代史。' AS description,
  '1. 演出全程谢绝摄影摄像。\n2. 请着装得体。\n3. 演出开始后谢绝入场。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '《只此青绿》舞蹈诗剧-杭州站' AS title, '杭州大剧院' AS venue, '杭州' AS city,
  800 AS total_seats, 800 AS available_seats, '2026-08-28' AS event_date, 1 AS status,
  'theater' AS category, 480 AS price, '中国东方演艺集团' AS artist,
  '以《千里江山图》为灵感的现象级舞蹈诗剧。青绿千载，山河无垠，展卷入画。' AS description,
  '1. 演出时长120分钟无中场。\n2. 1.2米以下儿童谢绝入场。\n3. 演出中请保持安静。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '梵高沉浸式光影艺术大展' AS title, '深圳市当代艺术馆' AS venue, '深圳' AS city,
  3000 AS total_seats, 3000 AS available_seats, '2026-12-20' AS event_date, 1 AS status,
  'exhibition' AS category, 128 AS price, NULL AS artist,
  '360度全景沉浸式光影空间，200余幅梵高名作数字化重生。《星夜》《向日葵》《麦田》在光影中流动。' AS description,
  '1. 展览期间每日10:00-21:00开放。\n2. 电子票扫码入场，有效期内任选一日。\n3. 馆内可拍照，禁用闪光灯。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '故宫·紫禁城建成606年特展' AS title, '故宫博物院午门展厅' AS venue, '北京' AS city,
  5000 AS total_seats, 5000 AS available_seats, '2026-11-30' AS event_date, 1 AS status,
  'exhibition' AS category, 60 AS price, NULL AS artist,
  '集中展出故宫院藏珍品450余件，含多件首次公开展出的一级文物。' AS description,
  '1. 凭身份证预约入场。\n2. 展厅内禁止饮食。\n3. 请配合安检。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '《流浪地球3》IMAX 首映礼' AS title, '成都万象城影城IMAX厅' AS venue, '成都' AS city,
  350 AS total_seats, 350 AS available_seats, '2026-10-01' AS event_date, 1 AS status,
  'movie' AS category, 168 AS price, NULL AS artist,
  '中国科幻里程碑之作首映礼场次，主创团队映后见面交流。IMAX巨幕呈现太空电梯震撼场面。' AS description,
  '1. 电影票售出不退不换。\n2. 映后交流环节约30分钟。\n3. 请提前15分钟入场。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '陈奕迅 FEAR AND DREAMS 巡回演唱会-成都站' AS title, '成都凤凰山体育公园' AS venue, '成都' AS city,
  1600 AS total_seats, 1600 AS available_seats, '2026-09-26' AS event_date, 1 AS status,
  'concert' AS category, 980 AS price, '陈奕迅' AS artist,
  'Eason 时隔三年再启巡演。《十年》《浮夸》《孤勇者》，恐惧与梦想的音乐对话。' AS description,
  '1. 实名购票，人证合一入场。\n2. 山顶位置视线可能部分遮挡，购票前请确认。\n3. 演出不设中场休息。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '德云社相声专场-天津站' AS title, '天津人民体育馆' AS venue, '天津' AS city,
  900 AS total_seats, 900 AS available_seats, '2026-08-30' AS event_date, 1 AS status,
  'theater' AS category, 299 AS price, '德云社' AS artist,
  '德云社男团天津专场，传统相声与新活轮番上阵，笑点密集不断。' AS description,
  '1. 一人一票对号入座。\n2. 演出时长约3小时。\n3. 禁止录音录像。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '2026 英雄联盟全球总决赛-入围赛' AS title, '武汉光谷国际网球中心' AS venue, '武汉' AS city,
  1400 AS total_seats, 1400 AS available_seats, '2026-10-10' AS event_date, 1 AS status,
  'esports' AS category, 480 AS price, NULL AS artist,
  'S16全球总决赛中国主场。全球十六支顶级战队集结，见证新王加冕之路。' AS description,
  '1. 实名制购票。\n2. 单日票可观看当日全部对局。\n3. 场内提供官方应援物。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '薛之谦「天外来物」巡回演唱会-南京站' AS title, '南京奥体中心体育场' AS venue, '南京' AS city,
  1700 AS total_seats, 1700 AS available_seats, '2026-09-05' AS event_date, 1 AS status,
  'concert' AS category, 580 AS price, '薛之谦' AS artist,
  '天外来物降落南京。《演员》《丑八怪》《天外来物》，段子与深情齐飞的谦式现场。' AS description,
  '1. 实名制购票入场。\n2. 售出不退不换。\n3. 请勿携带大型应援牌。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '音乐剧《剧院魅影》中文版-上海站' AS title, '上海文化广场' AS venue, '上海' AS city,
  700 AS total_seats, 700 AS available_seats, '2026-11-15' AS event_date, 1 AS status,
  'theater' AS category, 680 AS price, NULL AS artist,
  '韦伯经典音乐剧官方中文版。水晶吊灯、地下湖泊、面具之下的爱与孤独。' AS description,
  '1. 演出时长150分钟含中场。\n2. 儿童需购全票入场。\n3. 剧场内禁止摄影。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  'teamLab 无界美术馆常设展' AS title, 'teamLab无界美术馆·上海' AS venue, '上海' AS city,
  4000 AS total_seats, 4000 AS available_seats, '2026-12-31' AS event_date, 1 AS status,
  'exhibition' AS category, 229 AS price, NULL AS artist,
  '没有地图的美术馆。作品走出房间、与人互动，在无界的世界中徘徊、探索、发现。' AS description,
  '1. 指定日期票，过期作废。\n2. 建议穿着浅色衣物提升观展体验。\n3. 馆内地面有镜面区域，不建议穿裙装。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '张学友60+巡回演唱会-深圳站' AS title, '深圳湾体育中心' AS venue, '深圳' AS city,
  1900 AS total_seats, 1900 AS available_seats, '2026-10-24' AS event_date, 1 AS status,
  'concert' AS category, 1080 AS price, '张学友' AS artist,
  '歌神张学友60+巡演。四十年金曲串联，《吻别》《一千个伤心的理由》《她来听我的演唱会》，一开口就是青春。' AS description,
  '1. 实名制购票，一张身份证限购2张。\n2. 演出票不支持转赠。\n3. 请提前2小时到场安检。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '《哈利·波特与魔法石》电影交响音乐会' AS title, '广州大剧院' AS venue, '广州' AS city,
  550 AS total_seats, 550 AS available_seats, '2026-09-19' AS event_date, 1 AS status,
  'movie' AS category, 380 AS price, NULL AS artist,
  '大银幕高清放映全片，交响乐团现场演奏约翰·威廉姆斯经典配乐。魔法世界与现场音乐的双重沉浸。' AS description,
  '1. 演出含20分钟中场休息。\n2. 儿童一律凭票入场。\n3. 可穿魔法袍观演。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

INSERT INTO tickets (title, venue, city, total_seats, available_seats, event_date, status, category, price, artist, description, notice)
SELECT * FROM (SELECT
  '西安城墙新春灯会' AS title, '西安城墙景区' AS venue, '西安' AS city,
  6000 AS total_seats, 6000 AS available_seats, '2026-12-25' AS event_date, 1 AS status,
  'exhibition' AS category, 100 AS price, NULL AS artist,
  '千年古城墙上的大型主题灯会，数百组花灯点亮古都夜空。汉服游园、非遗市集、投壶猜谜。' AS description,
  '1. 灯会开放时间17:00-22:30。\n2. 门票当日有效。\n3. 景区内明火管制。' AS notice
) t WHERE NOT EXISTS (SELECT 1 FROM tickets WHERE title = t.title);

-- 给已有的老票补 city（老数据 city 为默认值'北京'即可，不强制更新）
SELECT COUNT(*) AS total_tickets FROM tickets;
