#ifndef HYPERTICKET_CATALOG_REPOSITORY_HPP
#define HYPERTICKET_CATALOG_REPOSITORY_HPP

#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>
#include <string>

#include "../MysqlStmt.hpp"

namespace hyperticket
{
    class CatalogRepository
    {
    public:
        Json::Value home(MYSQL *conn, int64_t userId)
        {
            MysqlStmt st(conn,
                "SELECT e.id,e.title,e.subtitle,e.category,e.city,e.artist,e.cover_path,e.home_section,"
                "e.ranking_weight,e.real_name_required,MIN(s.starts_at),MIN(s.sale_start_at),"
                "MIN(t.price_minor),COALESCE((SELECT SUM(COALESCE(tk2.available_seats,0)) FROM event_sessions s2 LEFT JOIN tickets tk2 ON tk2.id=s2.legacy_ticket_id WHERE s2.event_id=e.id),0),COUNT(DISTINCT s.id),"
                "MAX(v.name),MAX(v.longitude),MAX(v.latitude),"
                "EXISTS(SELECT 1 FROM event_favorites f WHERE f.event_id=e.id AND f.user_id=?),"
                "COALESCE((SELECT SUM(r.quantity) FROM reservations r JOIN event_sessions es ON es.legacy_ticket_id=r.ticket_id "
                "WHERE es.event_id=e.id AND r.status IN ('PENDING','CONFIRMED')),0) "
                "FROM events e JOIN event_sessions s ON s.event_id=e.id "
                "JOIN venues v ON v.id=s.venue_id LEFT JOIN tickets tk ON tk.id=s.legacy_ticket_id LEFT JOIN ticket_tiers t ON t.session_id=s.id "
                "WHERE e.status='PUBLISHED' AND s.status<>'CANCELLED' "
                "GROUP BY e.id ORDER BY e.ranking_weight DESC,MIN(s.starts_at),e.id");
            Json::Value out(Json::arrayValue);
            if (!st.ok()) return out;
            st.bindInt(0, userId);
            if (!st.execute() || !st.bindResults(20)) return out;
            while (st.fetch())
            {
                Json::Value e;
                e["event_id"] = Json::Int64(st.getInt(0));
                e["title"] = st.getString(1); e["subtitle"] = st.getString(2);
                e["category"] = st.getString(3); e["city"] = st.getString(4);
                e["artist"] = st.getString(5); e["cover_path"] = st.getString(6);
                e["home_section"] = st.getString(7); e["ranking_weight"] = Json::Int64(st.getInt(8));
                e["real_name_required"] = st.getInt(9) != 0;
                e["next_session_at"] = st.getString(10); e["sale_start_at"] = st.getString(11);
                e["min_price_minor"] = Json::Int64(st.getInt(12));
                e["available_inventory"] = Json::Int64(st.getInt(13));
                e["session_count"] = Json::Int64(st.getInt(14)); e["venue_name"] = st.getString(15);
                e["longitude"] = std::stod(st.getString(16).empty() ? "0" : st.getString(16));
                e["latitude"] = std::stod(st.getString(17).empty() ? "0" : st.getString(17));
                e["favorite"] = st.getInt(18) != 0; e["popularity"] = Json::Int64(st.getInt(19));
                out.append(e);
            }
            return out;
        }

        bool detail(MYSQL *conn, int64_t eventId, int64_t userId, Json::Value &out)
        {
            MysqlStmt e(conn,
                "SELECT id,title,subtitle,category,city,artist,organizer,COALESCE(description,''),"
                "COALESCE(notice,''),cover_path,real_name_required,home_section,ranking_weight,"
                "EXISTS(SELECT 1 FROM event_favorites f WHERE f.event_id=events.id AND f.user_id=?) "
                "FROM events WHERE id=? AND status='PUBLISHED'");
            if (!e.ok()) return false;
            e.bindInt(0, userId); e.bindInt(1, eventId);
            if (!e.execute() || !e.bindResults(14) || !e.fetch()) return false;
            out["event_id"] = Json::Int64(e.getInt(0)); out["title"] = e.getString(1);
            out["subtitle"] = e.getString(2); out["category"] = e.getString(3);
            out["city"] = e.getString(4); out["artist"] = e.getString(5);
            out["organizer"] = e.getString(6); out["description"] = e.getString(7);
            out["notice"] = e.getString(8); out["cover_path"] = e.getString(9);
            out["real_name_required"] = e.getInt(10) != 0;
            out["home_section"] = e.getString(11); out["ranking_weight"] = Json::Int64(e.getInt(12));
            out["favorite"] = e.getInt(13) != 0;
            out["sessions"] = Json::Value(Json::arrayValue);

            MysqlStmt s(conn,
                "SELECT s.id,s.legacy_ticket_id,s.session_name,s.sale_start_at,s.sale_end_at,s.starts_at,"
                "COALESCE(s.ends_at,''),s.status,v.id,v.name,v.city,v.address,v.longitude,v.latitude,"
                "COALESCE(h.id,0),COALESCE(h.name,''),COALESCE(h.hall_format,''),"
                "COALESCE(tk.available_seats,0) FROM event_sessions s JOIN venues v ON v.id=s.venue_id "
                "LEFT JOIN halls h ON h.id=s.hall_id LEFT JOIN tickets tk ON tk.id=s.legacy_ticket_id "
                "WHERE s.event_id=? ORDER BY s.starts_at");
            if (!s.ok()) return false;
            s.bindInt(0, eventId);
            if (!s.execute() || !s.bindResults(18)) return false;
            while (s.fetch())
            {
                Json::Value row;
                row["session_id"] = Json::Int64(s.getInt(0)); row["ticket_id"] = Json::Int64(s.getInt(1));
                row["name"] = s.getString(2); row["sale_start_at"] = s.getString(3);
                row["sale_end_at"] = s.getString(4); row["starts_at"] = s.getString(5);
                row["ends_at"] = s.getString(6); row["status"] = s.getString(7);
                row["venue_id"] = Json::Int64(s.getInt(8)); row["venue_name"] = s.getString(9);
                row["city"] = s.getString(10); row["address"] = s.getString(11);
                row["longitude"] = std::stod(s.getString(12)); row["latitude"] = std::stod(s.getString(13));
                row["hall_id"] = Json::Int64(s.getInt(14)); row["hall_name"] = s.getString(15);
                row["hall_format"] = s.getString(16); row["available_inventory"] = Json::Int64(s.getInt(17));
                row["tiers"] = tiers(conn, s.getInt(0));
                out["sessions"].append(row);
            }
            if (userId > 0)
            {
                MysqlStmt h(conn, "INSERT INTO browsing_history(user_id,event_id) VALUES(?,?) "
                                  "ON DUPLICATE KEY UPDATE view_count=view_count+1,last_viewed_at=NOW(3)");
                if (h.ok()) { h.bindInt(0,userId); h.bindInt(1,eventId); h.execute(); }
            }
            return true;
        }

        Json::Value tiers(MYSQL *conn, int64_t sessionId)
        {
            Json::Value out(Json::arrayValue);
            MysqlStmt st(conn, "SELECT id,name,price_minor,currency,inventory,available_inventory,purchase_limit,seat_mode "
                               "FROM ticket_tiers WHERE session_id=? ORDER BY sort_order,id");
            if (!st.ok()) return out;
            st.bindInt(0, sessionId);
            if (!st.execute() || !st.bindResults(8)) return out;
            while(st.fetch()) { Json::Value x; x["tier_id"]=Json::Int64(st.getInt(0)); x["name"]=st.getString(1);
                x["price_minor"]=Json::Int64(st.getInt(2)); x["currency"]=st.getString(3);
                x["inventory"]=Json::Int64(st.getInt(4)); x["available_inventory"]=Json::Int64(st.getInt(5));
                x["purchase_limit"]=Json::Int64(st.getInt(6)); x["seat_mode"]=st.getString(7); out.append(x); }
            return out;
        }

        bool setFavorite(MYSQL *conn, int64_t userId, int64_t eventId, bool add)
        {
            MysqlStmt st(conn, add ? "INSERT IGNORE INTO event_favorites(user_id,event_id) VALUES(?,?)"
                                   : "DELETE FROM event_favorites WHERE user_id=? AND event_id=?");
            if (!st.ok()) return false; st.bindInt(0,userId); st.bindInt(1,eventId); return st.execute();
        }

        Json::Value history(MYSQL *conn, int64_t userId)
        {
            Json::Value out(Json::arrayValue);
            MysqlStmt st(conn, "SELECT e.id,e.title,e.category,e.cover_path,h.view_count,h.last_viewed_at "
                               "FROM browsing_history h JOIN events e ON e.id=h.event_id WHERE h.user_id=? "
                               "ORDER BY h.last_viewed_at DESC LIMIT 50");
            if(!st.ok()) return out; st.bindInt(0,userId);
            if(!st.execute()||!st.bindResults(6)) return out;
            while(st.fetch()){Json::Value x;x["event_id"]=Json::Int64(st.getInt(0));x["title"]=st.getString(1);
                x["category"]=st.getString(2);x["cover_path"]=st.getString(3);x["view_count"]=Json::Int64(st.getInt(4));
                x["last_viewed_at"]=st.getString(5);out.append(x);} return out;
        }

        Json::Value profile(MYSQL *conn, int64_t userId)
        {
            MysqlStmt init(conn,"INSERT IGNORE INTO user_profiles(user_id,display_name) SELECT id,username FROM users WHERE id=?");
            if(init.ok()){init.bindInt(0,userId);init.execute();}
            Json::Value out;
            MysqlStmt st(conn,"SELECT u.id,u.username,u.tel,IFNULL(u.email,''),u.email_verified_at IS NOT NULL,"
                              "u.phone_verified_at IS NOT NULL,p.display_name,p.avatar_path,p.gender,IFNULL(p.birthday,''),p.city,p.bio "
                              "FROM users u JOIN user_profiles p ON p.user_id=u.id WHERE u.id=?");
            if(!st.ok()) return out;st.bindInt(0,userId);
            if(!st.execute()||!st.bindResults(12)||!st.fetch()) return out;
            out["user_id"]=Json::Int64(st.getInt(0));out["username"]=st.getString(1);out["tel"]=st.getString(2);
            out["email"]=st.getString(3);out["email_verified"]=st.getInt(4)!=0;out["phone_verified"]=st.getInt(5)!=0;
            out["display_name"]=st.getString(6);out["avatar_path"]=st.getString(7);out["gender"]=st.getString(8);
            out["birthday"]=st.getString(9);out["city"]=st.getString(10);out["bio"]=st.getString(11);return out;
        }

        bool updateProfile(MYSQL *conn,int64_t userId,const Json::Value &req)
        {
            MysqlStmt st(conn,"INSERT INTO user_profiles(user_id,display_name,avatar_path,gender,birthday,city,bio) "
                              "VALUES(?,?,?,?,NULLIF(?,''),?,?) ON DUPLICATE KEY UPDATE display_name=VALUES(display_name),"
                              "avatar_path=VALUES(avatar_path),gender=VALUES(gender),birthday=VALUES(birthday),city=VALUES(city),bio=VALUES(bio)");
            if(!st.ok()) return false;st.bindInt(0,userId);st.bindString(1,req.get("display_name","").asString().substr(0,64));
            st.bindString(2,req.get("avatar_path","/media/avatars/default.webp").asString().substr(0,512));
            std::string gender=req.get("gender","UNSPECIFIED").asString();if(gender!="MALE"&&gender!="FEMALE")gender="UNSPECIFIED";
            st.bindString(3,gender);st.bindString(4,req.get("birthday","").asString());st.bindString(5,req.get("city","").asString().substr(0,32));
            st.bindString(6,req.get("bio","").asString().substr(0,255));return st.execute();
        }

        Json::Value attendees(MYSQL *conn,int64_t userId)
        {
            Json::Value out(Json::arrayValue);MysqlStmt st(conn,"SELECT id,name,id_type,id_number_masked,phone_masked,is_default "
                "FROM attendees WHERE user_id=? AND deleted_at IS NULL ORDER BY is_default DESC,id");if(!st.ok())return out;
            st.bindInt(0,userId);if(!st.execute()||!st.bindResults(6))return out;while(st.fetch()){Json::Value x;
            x["attendee_id"]=Json::Int64(st.getInt(0));x["name"]=st.getString(1);x["id_type"]=st.getString(2);
            x["id_number_masked"]=st.getString(3);x["phone_masked"]=st.getString(4);x["is_default"]=st.getInt(5)!=0;out.append(x);}return out;
        }

        bool addAttendee(MYSQL *conn,int64_t userId,const std::string &name,const std::string &type,const std::string &cipher,
                         const std::string &hash,const std::string &masked,const std::string &phone,bool isDefault)
        {
            if(isDefault){MysqlStmt clear(conn,"UPDATE attendees SET is_default=0 WHERE user_id=?");if(clear.ok()){clear.bindInt(0,userId);clear.execute();}}
            MysqlStmt st(conn,"INSERT INTO attendees(user_id,name,id_type,id_number_ciphertext,id_number_hash,id_number_masked,phone_masked,is_default) VALUES(?,?,?,?,?,?,?,?)");
            if(!st.ok())return false;st.bindInt(0,userId);st.bindString(1,name);st.bindString(2,type);st.bindString(3,cipher);
            st.bindString(4,hash);st.bindString(5,masked);st.bindString(6,phone);st.bindInt(7,isDefault?1:0);return st.execute();
        }

        bool deleteAttendee(MYSQL *conn,int64_t userId,int64_t attendeeId)
        { MysqlStmt st(conn,"UPDATE attendees SET deleted_at=NOW(3),is_default=0 WHERE id=? AND user_id=? AND deleted_at IS NULL");
          if(!st.ok())return false;st.bindInt(0,attendeeId);st.bindInt(1,userId);return st.execute()&&st.affectedRows()==1; }

        Json::Value reminders(MYSQL *conn,int64_t userId)
        {
            Json::Value out(Json::arrayValue);MysqlStmt st(conn,"SELECT r.id,r.event_id,e.title,e.cover_path,r.remind_at,r.status,r.attempt_count,r.last_error "
                "FROM sale_reminders r JOIN events e ON e.id=r.event_id WHERE r.user_id=? ORDER BY r.created_at DESC");if(!st.ok())return out;
            st.bindInt(0,userId);if(!st.execute()||!st.bindResults(8))return out;while(st.fetch()){Json::Value x;x["reminder_id"]=Json::Int64(st.getInt(0));
            x["event_id"]=Json::Int64(st.getInt(1));x["title"]=st.getString(2);x["cover_path"]=st.getString(3);x["remind_at"]=st.getString(4);
            x["status"]=st.getString(5);x["attempt_count"]=Json::Int64(st.getInt(6));x["last_error"]=st.getString(7);out.append(x);}return out;
        }

        bool addReminder(MYSQL *conn,int64_t userId,int64_t eventId)
        {
            MysqlStmt st(conn,"INSERT INTO sale_reminders(user_id,event_id,session_id,email,remind_at,next_attempt_at) "
              "SELECT u.id,e.id,s.id,u.email,s.sale_start_at,s.sale_start_at FROM users u JOIN events e ON e.id=? "
              "JOIN event_sessions s ON s.event_id=e.id WHERE u.id=? AND u.email_verified_at IS NOT NULL "
              "ORDER BY s.sale_start_at LIMIT 1 ON DUPLICATE KEY UPDATE status='PENDING',session_id=VALUES(session_id),"
              "email=VALUES(email),remind_at=VALUES(remind_at),next_attempt_at=VALUES(next_attempt_at),attempt_count=0,last_error=''");
            if(!st.ok())return false;st.bindInt(0,eventId);st.bindInt(1,userId);return st.execute()&&st.affectedRows()>0;
        }

        bool cancelReminder(MYSQL *conn,int64_t userId,int64_t reminderId)
        {MysqlStmt st(conn,"UPDATE sale_reminders SET status='CANCELLED' WHERE id=? AND user_id=? AND status IN('PENDING','FAILED')");
         if(!st.ok())return false;st.bindInt(0,reminderId);st.bindInt(1,userId);return st.execute()&&st.affectedRows()==1;}
    };
}

#endif
