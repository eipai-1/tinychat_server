#include <stdexcept>

#include "spdlog/spdlog.h"
#include "db/sql_conn_RAII.hpp"

namespace tcs {
namespace db {
SqlConnRAII::SqlConnRAII() : pool_(nullptr), sql_(nullptr) {
    pool_ = SqlConnPool::instance();
    sql_ = pool_->getConn();
}

Connection* SqlConnRAII::getSql() {
    if (!sql_->isValid()) {
        spdlog::warn("Sql in SqlConnRAII is invalid. Getting a new Sql");
        sql_ = pool_->getConn();
    }
    return sql_;
}

SqlConnRAII::~SqlConnRAII() {
    if (sql_->isValid()) {
        pool_->freeConn(sql_);
    } else {
        spdlog::warn("Sql in SqlConnRAII is invalid. Not returning to pool");
    }
}

void SqlConnRAII::bind_all_param(PrepStmt* pstmt, int idx, const std::string& str) {
    pstmt->setString(idx, str);
}

}  // namespace db
}  // namespace tcs