#include <stdexcept>

#include "spdlog/spdlog.h"
#include "db/sql_conn_RAII.hpp"

namespace tcs {
namespace db {
SqlConnRAII::SqlConnRAII() : pool_(nullptr), sql_(nullptr) {
    pool_ = SqlConnPool::instance();
    sql_ = pool_->getConn();
}

SqlConnRAII::~SqlConnRAII() {
    if (sql_->isValid()) {
        if (!sql_->getAutoCommit()) {
            sql_->setAutoCommit(true);
        }
        pool_->freeConn(sql_);
    } else {
        spdlog::warn("Sql in SqlConnRAII is invalid. Not returning to pool");
    }
}
}  // namespace db
}  // namespace tcs