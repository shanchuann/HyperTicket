#ifndef HYPERTICKET_MYSQL_STMT_HPP
#define HYPERTICKET_MYSQL_STMT_HPP

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdlib>

namespace hyperticket
{
    // 轻量 RAII 封装 mysql 预处理语句，用于参数化查询以根除 SQL 注入。
    // 用法：
    //   MysqlStmt st(conn, "SELECT id FROM users WHERE tel = ?");
    //   st.bindString(0, tel);
    //   if (st.execute()) { while (st.fetch()) { ... st.getString(0) ... } }
    class MysqlStmt
    {
    public:
        MysqlStmt(MYSQL *conn, const std::string &sql)
            : stmt_(mysql_stmt_init(conn))
        {
            if (stmt_ && mysql_stmt_prepare(stmt_, sql.c_str(), sql.size()) == 0)
            {
                paramCount_ = mysql_stmt_param_count(stmt_);
                params_.resize(paramCount_);
                lengths_.resize(paramCount_, 0);
                std::memset(params_.data(), 0, sizeof(MYSQL_BIND) * params_.size());
                prepared_ = true;
            }
        }

        ~MysqlStmt()
        {
            if (stmt_) mysql_stmt_close(stmt_);
        }

        MysqlStmt(const MysqlStmt &) = delete;
        MysqlStmt &operator=(const MysqlStmt &) = delete;

        bool ok() const { return prepared_; }

        // 绑定字符串参数（值被复制保存，生命周期由本对象管理）。
        void bindString(unsigned idx, const std::string &val)
        {
            if (idx >= paramCount_) return;
            strStore_.resize(paramCount_);
            strStore_[idx] = val;
            lengths_[idx] = strStore_[idx].size();
            MYSQL_BIND &b = params_[idx];
            std::memset(&b, 0, sizeof(b));
            b.buffer_type = MYSQL_TYPE_STRING;
            b.buffer = const_cast<char *>(strStore_[idx].data());
            b.buffer_length = strStore_[idx].size();
            b.length = &lengths_[idx];
        }

        // 绑定整型参数。
        void bindInt(unsigned idx, long long val)
        {
            if (idx >= paramCount_) return;
            intStore_.resize(paramCount_);
            intStore_[idx] = val;
            MYSQL_BIND &b = params_[idx];
            std::memset(&b, 0, sizeof(b));
            b.buffer_type = MYSQL_TYPE_LONGLONG;
            b.buffer = &intStore_[idx];
            b.is_null = nullptr;
            b.length = nullptr;
        }

        // 执行（INSERT/UPDATE 或 SELECT）。SELECT 后用 fetch()/getXxx 取结果。
        bool execute()
        {
            if (!prepared_) return false;
            if (paramCount_ > 0 && mysql_stmt_bind_param(stmt_, params_.data()) != 0)
            {
                return false;
            }
            if (mysql_stmt_execute(stmt_) != 0)
            {
                return false;
            }
            return true;
        }

        // 为 SELECT 结果准备输出缓冲。numCols: 结果列数。
        bool bindResults(unsigned numCols)
        {
            numCols_ = numCols;
            resBuf_.assign(numCols, std::string());
            resBufRaw_.assign(numCols, std::vector<char>(512));
            resLen_.assign(numCols, 0);
            resIsNull_.reset(new bool[numCols]());
            resBind_.assign(numCols, MYSQL_BIND());
            std::memset(resBind_.data(), 0, sizeof(MYSQL_BIND) * numCols);
            for (unsigned i = 0; i < numCols; ++i)
            {
                resBind_[i].buffer_type = MYSQL_TYPE_STRING;
                resBind_[i].buffer = resBufRaw_[i].data();
                resBind_[i].buffer_length = resBufRaw_[i].size();
                resBind_[i].length = &resLen_[i];
                resBind_[i].is_null = &resIsNull_[i];
            }
            if (mysql_stmt_bind_result(stmt_, resBind_.data()) != 0) return false;
            if (mysql_stmt_store_result(stmt_) != 0) return false;
            return true;
        }

        // 取下一行；无更多行返回 false。
        bool fetch()
        {
            int rc = mysql_stmt_fetch(stmt_);
            if (rc != 0 && rc != MYSQL_DATA_TRUNCATED) return false;
            for (unsigned i = 0; i < numCols_; ++i)
            {
                if (resIsNull_[i])
                {
                    resBuf_[i].clear();
                }
                else
                {
                    unsigned long len = resLen_[i];
                    if (len > resBufRaw_[i].size()) len = resBufRaw_[i].size();
                    resBuf_[i].assign(resBufRaw_[i].data(), len);
                }
            }
            return true;
        }

        std::string getString(unsigned col) const
        {
            return col < resBuf_.size() ? resBuf_[col] : std::string();
        }
        long long getInt(unsigned col) const
        {
            if (col >= resBuf_.size() || resBuf_[col].empty()) return 0;
            return std::strtoll(resBuf_[col].c_str(), nullptr, 10);
        }

        my_ulonglong numRows() { return mysql_stmt_num_rows(stmt_); }
        my_ulonglong affectedRows() { return mysql_stmt_affected_rows(stmt_); }

    private:
        MYSQL_STMT *stmt_ = nullptr;
        bool prepared_ = false;
        unsigned paramCount_ = 0;
        unsigned numCols_ = 0;

        std::vector<MYSQL_BIND> params_;
        std::vector<unsigned long> lengths_;
        std::vector<std::string> strStore_;
        std::vector<long long> intStore_;

        std::vector<MYSQL_BIND> resBind_;
        std::vector<std::vector<char>> resBufRaw_;
        std::vector<std::string> resBuf_;
        std::vector<unsigned long> resLen_;
        std::unique_ptr<bool[]> resIsNull_;
    };
} // namespace hyperticket
#endif // HYPERTICKET_MYSQL_STMT_HPP
