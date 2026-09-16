#include "../../include/service/TicketService.hpp"

#include <algorithm>
#include <regex>

#include "../../../Common/include/Errors.hpp"
#include "../../../Common/include/Protocol.hpp"
#include "../../include/ServiceUtil.hpp"
#include "../../include/service/Txn.hpp"
#include "../../../ChronoLite/include/Logger.hpp"

namespace hyperticket
{
    namespace
    {
        bool resolveOptional(ISessionManager *sessions, const Json::Value &req,
                             std::string &tel, int64_t &userId)
        {
            const std::string token = req.get(field::kToken, "").asString();
            if (token.empty()) { userId = 0; return true; }
            if (!sessions->resolve(token, nowMs(), tel, userId))
            {
                tel.clear();
                userId = 0;
            }
            return true;
        }

        bool validIdentityType(const std::string &value)
        {
            return value == "PRC_ID" || value == "PASSPORT" ||
                   value == "HK_MACAO_PERMIT" || value == "TAIWAN_PERMIT";
        }

        bool execSimple(MYSQL *conn, const std::string &sql)
        {
            return mysql_query(conn, sql.c_str()) == 0;
        }
    }

    Json::Value TicketService::catalogHome(const Json::Value &req)
    {
        std::string tel; int64_t userId = 0;
        if (!resolveOptional(sessions_, req, tel, userId)) return makeError(err::kUnauthorized);
        MYSQL *conn = nullptr; shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);
        Json::Value res = makeOk(); res["events"] = catalogRepo_.home(conn, userId);
        res["categories"] = Json::Value(Json::arrayValue);
        for (const char *value : {"movie","concert","performance","comedy","exhibition","esports","sports"})
            res["categories"].append(value);
        return res;
    }

    Json::Value TicketService::eventDetail(const Json::Value &req)
    {
        const int64_t eventId = getIntField(req, "event_id", 0);
        if (eventId <= 0) return makeError(err::kInvalidInput);
        std::string tel; int64_t userId = 0;
        if (!resolveOptional(sessions_, req, tel, userId)) return makeError(err::kUnauthorized);
        MYSQL *conn = nullptr; shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);
        Json::Value event;
        if (!catalogRepo_.detail(conn, eventId, userId, event)) return makeError(err::kTicketNotFound);
        Json::Value res = makeOk(); res["event"] = event; return res;
    }

    Json::Value TicketService::profileGet(const Json::Value &req)
    {
        std::string tel; int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken,"").asString(),nowMs(),tel,userId)) return makeError(err::kUnauthorized);
        MYSQL *conn=nullptr; shanchuan::ConnectionGuard guard(&conn,pool_); if(!conn)return makeError(err::kDbUnavailable);
        Json::Value res=makeOk(); res["profile"]=catalogRepo_.profile(conn,userId); return res;
    }

    Json::Value TicketService::profileUpdate(const Json::Value &req)
    {
        std::string tel; int64_t userId=0;
        if(!sessions_->resolve(req.get(field::kToken,"").asString(),nowMs(),tel,userId))return makeError(err::kUnauthorized);
        MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return makeError(err::kDbUnavailable);
        if(!catalogRepo_.updateProfile(conn,userId,req))return makeError(err::kDbUpdate);
        Json::Value res=makeOk();res["profile"]=catalogRepo_.profile(conn,userId);return res;
    }

    Json::Value TicketService::attendeeList(const Json::Value &req)
    {
        std::string tel;int64_t userId=0;if(!sessions_->resolve(req.get(field::kToken,"").asString(),nowMs(),tel,userId))return makeError(err::kUnauthorized);
        MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return makeError(err::kDbUnavailable);
        Json::Value res=makeOk();res["attendees"]=catalogRepo_.attendees(conn,userId);return res;
    }

    Json::Value TicketService::attendeeMutate(const Json::Value &req)
    {
        std::string tel;int64_t userId=0;if(!sessions_->resolve(req.get(field::kToken,"").asString(),nowMs(),tel,userId))return makeError(err::kUnauthorized);
        const std::string action=req.get(field::kAction,"").asString();MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);
        if(!conn)return makeError(err::kDbUnavailable);
        bool ok=false;
        if(action=="delete") ok=catalogRepo_.deleteAttendee(conn,userId,getIntField(req,"attendee_id",0));
        else if(action=="add")
        {
            const std::string name=req.get("name","").asString(),type=req.get("id_type","").asString(),number=req.get("id_number","").asString();
            std::string phone=req.get("phone","").asString();
            if(name.empty()||name.size()>64||number.size()<5||number.size()>64||!validIdentityType(type))return makeError(err::kInvalidInput);
            const std::string cipher=encryptSensitive(number);if(cipher.empty())return makeError("ENCRYPTION_FAILED");
            const std::string phoneMasked=phone.size()>=7?phone.substr(0,3)+"****"+phone.substr(phone.size()-4):phone;
            ok=catalogRepo_.addAttendee(conn,userId,name,type,cipher,sha256Hex(number),maskIdentity(number),phoneMasked,req.get("is_default",false).asBool());
        }
        else return makeError(err::kInvalidInput);
        if(!ok)return makeError(err::kDbUpdate);Json::Value res=makeOk();res["attendees"]=catalogRepo_.attendees(conn,userId);return res;
    }

    Json::Value TicketService::browsingHistory(const Json::Value &req)
    {
        std::string tel;int64_t userId=0;if(!sessions_->resolve(req.get(field::kToken,"").asString(),nowMs(),tel,userId))return makeError(err::kUnauthorized);
        MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return makeError(err::kDbUnavailable);
        Json::Value res=makeOk();res["history"]=catalogRepo_.history(conn,userId);return res;
    }

    Json::Value TicketService::saleReminderList(const Json::Value &req)
    {
        std::string tel;int64_t userId=0;if(!sessions_->resolve(req.get(field::kToken,"").asString(),nowMs(),tel,userId))return makeError(err::kUnauthorized);
        MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return makeError(err::kDbUnavailable);
        Json::Value res=makeOk();res["reminders"]=catalogRepo_.reminders(conn,userId);return res;
    }

    Json::Value TicketService::saleReminderMutate(const Json::Value &req)
    {
        std::string tel;int64_t userId=0;if(!sessions_->resolve(req.get(field::kToken,"").asString(),nowMs(),tel,userId))return makeError(err::kUnauthorized);
        MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return makeError(err::kDbUnavailable);
        const std::string action=req.get(field::kAction,"").asString();bool ok=false;
        if(action=="add")ok=catalogRepo_.addReminder(conn,userId,getIntField(req,"event_id",0));
        else if(action=="cancel")ok=catalogRepo_.cancelReminder(conn,userId,getIntField(req,"reminder_id",0));
        else return makeError(err::kInvalidInput);
        if(!ok)return makeError(action=="add"?"VERIFIED_EMAIL_REQUIRED":err::kDbUpdate);
        Json::Value res=makeOk();res["reminders"]=catalogRepo_.reminders(conn,userId);return res;
    }

    Json::Value TicketService::eventFavorite(const Json::Value &req)
    {
        std::string tel;int64_t userId=0;if(!sessions_->resolve(req.get(field::kToken,"").asString(),nowMs(),tel,userId))return makeError(err::kUnauthorized);
        const int64_t eventId=getIntField(req,"event_id",0);const std::string action=req.get(field::kAction,"add").asString();
        if(eventId<=0||(action!="add"&&action!="remove"))return makeError(err::kInvalidInput);
        MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return makeError(err::kDbUnavailable);
        return catalogRepo_.setFavorite(conn,userId,eventId,action=="add")?makeOk():makeError(err::kDbUpdate);
    }

    Json::Value TicketService::adminCatalog(const Json::Value &req)
    {
        std::string username,error;if(!authorizeAdmin(req,username,error))return makeError(error);
        MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return makeError(err::kDbUnavailable);
        Json::Value res=makeOk();res["events"]=catalogRepo_.home(conn,0);
        auto query=[&](const char *sql,unsigned cols){Json::Value rows(Json::arrayValue);MysqlStmt st(conn,sql);if(!st.ok()||!st.execute()||!st.bindResults(cols))return rows;
            while(st.fetch()){Json::Value r;for(unsigned i=0;i<cols;++i)r[std::to_string(i)]=st.getString(i);rows.append(r);}return rows;};
        res["venues"]=query("SELECT id,name,venue_type,city,address,longitude,latitude,status FROM venues ORDER BY city,name",8);
        res["halls"]=query("SELECT id,venue_id,name,hall_format,seat_mode,capacity FROM halls ORDER BY venue_id,name",6);
        res["sessions"]=query("SELECT id,event_id,venue_id,COALESCE(hall_id,0),COALESCE(legacy_ticket_id,0),session_name,sale_start_at,sale_end_at,starts_at,status FROM event_sessions ORDER BY starts_at",10);
        res["tiers"]=query("SELECT id,session_id,name,price_minor,currency,inventory,available_inventory,purchase_limit,seat_mode FROM ticket_tiers ORDER BY session_id,sort_order",9);
        return res;
    }

    Json::Value TicketService::adminCatalogMutate(const Json::Value &req)
    {
        std::string username,error;if(!authorizeAdmin(req,username,error))return makeError(error);
        const std::string action=req.get(field::kAction,"").asString();MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return makeError(err::kDbUnavailable);
        bool ok=false;
        if(action=="venue_upsert")
        {
            MysqlStmt st(conn,"INSERT INTO venues(id,name,venue_type,city,address,longitude,latitude,status) VALUES(NULLIF(?,0),?,?,?,?,?,?,1) "
                              "ON DUPLICATE KEY UPDATE name=VALUES(name),venue_type=VALUES(venue_type),city=VALUES(city),address=VALUES(address),longitude=VALUES(longitude),latitude=VALUES(latitude)");
            if(st.ok()){st.bindInt(0,getIntField(req,"id",0));st.bindString(1,req.get("name","").asString());st.bindString(2,req.get("venue_type","VENUE").asString());
                st.bindString(3,req.get("city","").asString());st.bindString(4,req.get("address","").asString());st.bindString(5,req.get("longitude","0").asString());st.bindString(6,req.get("latitude","0").asString());ok=st.execute();}
        }
        else if(action=="hall_upsert")
        {
            MysqlStmt st(conn,"INSERT INTO halls(id,venue_id,name,hall_format,seat_mode,capacity) VALUES(NULLIF(?,0),?,?,?,?,?) ON DUPLICATE KEY UPDATE venue_id=VALUES(venue_id),name=VALUES(name),hall_format=VALUES(hall_format),seat_mode=VALUES(seat_mode),capacity=VALUES(capacity)");
            if(st.ok()){st.bindInt(0,getIntField(req,"id",0));st.bindInt(1,getIntField(req,"venue_id",0));st.bindString(2,req.get("name","").asString());st.bindString(3,req.get("hall_format","标准").asString());st.bindString(4,req.get("seat_mode","RESERVED").asString());st.bindInt(5,getIntField(req,"capacity",0));ok=st.execute();}
        }
        else if(action=="event_upsert")
        {
            MysqlStmt st(conn,"INSERT INTO events(id,title,category,subtitle,organizer,artist,description,notice,cover_path,city,real_name_required,status,home_section,ranking_weight) "
             "VALUES(NULLIF(?,0),?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE title=VALUES(title),category=VALUES(category),subtitle=VALUES(subtitle),organizer=VALUES(organizer),artist=VALUES(artist),description=VALUES(description),notice=VALUES(notice),cover_path=VALUES(cover_path),city=VALUES(city),real_name_required=VALUES(real_name_required),status=VALUES(status),home_section=VALUES(home_section),ranking_weight=VALUES(ranking_weight)");
            if(st.ok()){st.bindInt(0,getIntField(req,"id",0));st.bindString(1,req.get("title","").asString());st.bindString(2,req.get("category","concert").asString());st.bindString(3,req.get("subtitle","").asString());st.bindString(4,req.get("organizer","").asString());st.bindString(5,req.get("artist","").asString());st.bindString(6,req.get("description","").asString());st.bindString(7,req.get("notice","").asString());st.bindString(8,req.get("cover_path","").asString());st.bindString(9,req.get("city","").asString());st.bindInt(10,req.get("real_name_required",false).asBool());st.bindString(11,req.get("event_status","PUBLISHED").asString());st.bindString(12,req.get("home_section","RECOMMENDED").asString());st.bindInt(13,getIntField(req,"ranking_weight",0));ok=st.execute();}
        }
        else if(action=="tier_upsert")
        {
            MysqlStmt st(conn,"INSERT INTO ticket_tiers(id,session_id,name,price_minor,currency,inventory,available_inventory,purchase_limit,seat_mode,sort_order) VALUES(NULLIF(?,0),?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE name=VALUES(name),price_minor=VALUES(price_minor),currency=VALUES(currency),inventory=VALUES(inventory),available_inventory=LEAST(VALUES(available_inventory),VALUES(inventory)),purchase_limit=VALUES(purchase_limit),seat_mode=VALUES(seat_mode),sort_order=VALUES(sort_order)");
            if(st.ok()){st.bindInt(0,getIntField(req,"id",0));st.bindInt(1,getIntField(req,"session_id",0));st.bindString(2,req.get("name","").asString());st.bindInt(3,getIntField(req,"price_minor",0));st.bindString(4,req.get("currency","CNY").asString());st.bindInt(5,getIntField(req,"inventory",0));st.bindInt(6,getIntField(req,"available_inventory",getIntField(req,"inventory",0)));st.bindInt(7,getIntField(req,"purchase_limit",6));st.bindString(8,req.get("seat_mode","RESERVED").asString());st.bindInt(9,getIntField(req,"sort_order",0));ok=st.execute();}
        }
        else if(action=="session_create")
        {
            const std::string saleStart=req.get("sale_start_at","").asString(),saleEnd=req.get("sale_end_at","").asString(),starts=req.get("starts_at","").asString();
            if(saleStart.empty()||saleEnd.empty()||starts.empty()||!(saleStart<saleEnd&&saleEnd<starts))return makeError("INVALID_TIME_ORDER");
            Txn txn(conn);if(!txn.ok())return makeError(err::kDbBegin);
            MysqlStmt event(conn,"SELECT title,category,city,artist,cover_path,description,notice FROM events WHERE id=?");event.bindInt(0,getIntField(req,"event_id",0));
            MysqlStmt venue(conn,"SELECT name FROM venues WHERE id=?");venue.bindInt(0,getIntField(req,"venue_id",0));
            if(!event.execute()||!event.bindResults(7)||!event.fetch()||!venue.execute()||!venue.bindResults(1)||!venue.fetch()){txn.rollback();return makeError(err::kInvalidInput);}
            int capacity=static_cast<int>(getIntField(req,"capacity",100));std::string date=starts.substr(0,10);
            if(!ticketRepo_.insert(conn,event.getString(0),venue.getString(0),capacity,date,event.getString(4),event.getString(1),static_cast<int>(getIntField(req,"base_price",0)),event.getString(2),event.getString(3),event.getString(5),event.getString(6))){txn.rollback();return makeError(err::kDbInsert);}
            int64_t ticketId=ticketRepo_.lastInsertId(conn);
            MysqlStmt st(conn,"INSERT INTO event_sessions(event_id,venue_id,hall_id,legacy_ticket_id,session_name,sale_start_at,sale_end_at,starts_at,ends_at,status) VALUES(?,?,NULLIF(?,0),?,?,?,?,?,NULLIF(?,''),'SCHEDULED')");
            if(st.ok()){st.bindInt(0,getIntField(req,"event_id",0));st.bindInt(1,getIntField(req,"venue_id",0));st.bindInt(2,getIntField(req,"hall_id",0));st.bindInt(3,ticketId);st.bindString(4,req.get("name","").asString());st.bindString(5,saleStart);st.bindString(6,saleEnd);st.bindString(7,starts);st.bindString(8,req.get("ends_at","").asString());ok=st.execute();}
            if(ok&&req.get("seat_mode","RESERVED").asString()=="RESERVED") ok=seatRepo_.generate(conn,ticketId,capacity,static_cast<int>(getIntField(req,"base_price",0)));
            if(ok)ok=txn.commit();else txn.rollback();
        }
        else if(action=="copy_session")
        {
            const int64_t source=getIntField(req,"source_session_id",0);const std::string saleStart=req.get("sale_start_at","").asString(),saleEnd=req.get("sale_end_at","").asString(),starts=req.get("starts_at","").asString();
            if(!(saleStart<saleEnd&&saleEnd<starts))return makeError("INVALID_TIME_ORDER");
            MysqlStmt src(conn,"SELECT event_id,venue_id,COALESCE(hall_id,0),session_name FROM event_sessions WHERE id=?");src.bindInt(0,source);
            if(!src.execute()||!src.bindResults(4)||!src.fetch())return makeError(err::kInvalidInput);
            Json::Value create=req;create[field::kAction]="session_create";create["event_id"]=Json::Int64(src.getInt(0));create["venue_id"]=Json::Int64(src.getInt(1));create["hall_id"]=Json::Int64(src.getInt(2));create["name"]=src.getString(3)+" 副本";
            return adminCatalogMutate(create);
        }
        else return makeError(err::kInvalidInput);
        if(!ok)return makeError(err::kDbUpdate);stock_->invalidateTicketList();return makeOk();
    }

    Json::Value TicketService::adminReminderList(const Json::Value &req)
    {
        std::string username,error;if(!authorizeAdmin(req,username,error))return makeError(error);
        MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return makeError(err::kDbUnavailable);
        Json::Value rows(Json::arrayValue);MysqlStmt st(conn,"SELECT r.id,e.title,r.email,r.remind_at,r.status,r.attempt_count,r.last_error,r.updated_at FROM sale_reminders r JOIN events e ON e.id=r.event_id ORDER BY r.updated_at DESC LIMIT 200");
        if(!st.ok()||!st.execute()||!st.bindResults(8))return makeError(err::kDbQuery);while(st.fetch()){Json::Value x;x["reminder_id"]=Json::Int64(st.getInt(0));x["title"]=st.getString(1);x["email"]=st.getString(2);x["remind_at"]=st.getString(3);x["status"]=st.getString(4);x["attempt_count"]=Json::Int64(st.getInt(5));x["last_error"]=st.getString(6);x["updated_at"]=st.getString(7);rows.append(x);}Json::Value res=makeOk();res["reminders"]=rows;return res;
    }

    bool TicketService::dispatchSaleReminders()
    {
        if(!verificationProvider_)return false;MYSQL *conn=nullptr;shanchuan::ConnectionGuard guard(&conn,pool_);if(!conn)return false;
        MysqlStmt st(conn,"SELECT r.id,r.email,e.title,s.starts_at FROM sale_reminders r JOIN events e ON e.id=r.event_id LEFT JOIN event_sessions s ON s.id=r.session_id WHERE r.status IN('PENDING','FAILED') AND r.next_attempt_at<=NOW(3) ORDER BY r.next_attempt_at LIMIT 20");
        if(!st.ok()||!st.execute()||!st.bindResults(4))return false;
        struct Due{int64_t id;std::string email,title,starts;};std::vector<Due> due;while(st.fetch())due.push_back({st.getInt(0),st.getString(1),st.getString(2),st.getString(3)});
        for(const auto &item:due){MysqlStmt claim(conn,"UPDATE sale_reminders SET status='PROCESSING',attempt_count=attempt_count+1 WHERE id=? AND status IN('PENDING','FAILED')");claim.bindInt(0,item.id);if(!claim.execute()||claim.affectedRows()!=1)continue;
            std::string sendError;const bool sent=verificationProvider_->sendCode("EMAIL",item.email,item.title+" | "+item.starts,"SALE_REMINDER",sendError);
            MysqlStmt done(conn,sent?"UPDATE sale_reminders SET status='SENT',sent_at=NOW(3),last_error='' WHERE id=?":"UPDATE sale_reminders SET status=IF(attempt_count>=max_attempts,'FAILED','PENDING'),next_attempt_at=DATE_ADD(NOW(3),INTERVAL LEAST(3600,POW(2,attempt_count)*60) SECOND),last_error=? WHERE id=?");
            if(sent){done.bindInt(0,item.id);}else{done.bindString(0,sendError.substr(0,512));done.bindInt(1,item.id);}done.execute();
            MysqlStmt audit(conn,"INSERT INTO sale_reminder_audit(reminder_id,event,detail) VALUES(?,?,?)");audit.bindInt(0,item.id);audit.bindString(1,sent?"SENT":"FAILED");audit.bindString(2,sendError.substr(0,512));audit.execute();}
        return true;
    }
}
