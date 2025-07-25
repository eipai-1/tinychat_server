#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <optional>

#include "db/sql_conn_pool.hpp"
#include "utils/types.hpp"

using result_type = std::vector<std::map<std::string, std::optional<std::string>>>;
using PrepStmt = sql::PreparedStatement;

namespace tcs {
namespace db {

class SqlConnRAII {
public:
    SqlConnRAII();

    Connection* getSql();

    ~SqlConnRAII();

    void begin_transaction() { sql_->setAutoCommit(false); }

    void commit() { sql_->commit(); }

    void rollback() { sql_->rollback(); }

    void bind_all_param(PrepStmt* pstmt, int idx, const std::string& str);

    void bind_all_param(PrepStmt* pstmt, int idx, int value) { pstmt->setInt(idx, value); }

    void bind_all_param(PrepStmt* pstmt, int idx, u64 value) { pstmt->setUInt64(idx, value); }

    void bind_all_param(PrepStmt* pstmt, int idx, i64 value) { pstmt->setInt64(idx, value); }

    void bind_all_param(PrepStmt* pstmt, int idx) {}

    template <typename First, typename... Rest>
    void bind_all_param(PrepStmt* pstmt, int idx, const First& first, const Rest&... rest) {
        bind_all_param(pstmt, idx, first);
        bind_all_param(pstmt, ++idx, rest...);
    }

    // 与标准库作区分
    template <typename... Args>
    sql::ResultSet* execute_query(const std::string& sql_template, const Args&... args) {
        sql_ = getSql();
        std::unique_ptr<PrepStmt> pstmt(sql_->prepareStatement(sql_template));
        int idx = 0;
        bind_all_param(pstmt.get(), ++idx, args...);
        return pstmt->executeQuery();
    }

    template <typename... Args>
    int execute_update(const std::string& sql_template, const Args&... args) {
        sql_ = getSql();
        std::unique_ptr<PrepStmt> pstmt(sql_->prepareStatement(sql_template));
        int idx = 0;
        bind_all_param(pstmt.get(), ++idx, args...);
        return pstmt->executeUpdate();
    }

private:
    Connection* sql_;
    SqlConnPool* pool_;
};
}  // namespace db
}  // namespace tcs