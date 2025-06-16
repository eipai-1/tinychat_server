#pragma once

#include "pool/sql_conn_pool.hpp"
#include <mysql/mysql.h>
#include <vector>
#include <map>
#include <string>
#include <optional>

using result_type = std::vector<std::map<std::string, std::optional<std::string>>>;

class SqlConnRAII {
public:
    SqlConnRAII(SqlConnPool* pool) : pool_(pool), sql_(nullptr) { sql_ = pool->getConn(); }
    MYSQL* getSql() const { return sql_; }

    ~SqlConnRAII() {
        if (sql_) {
            pool_->freeConn(sql_);
        }
    }

    result_type query(const std::string& sql) {
        if (mysql_query(sql_, sql.c_str())) {
            std::cerr << "MySQL query error: " << mysql_error(sql_) << std::endl;
            throw std::runtime_error("MySQL query error: " + std::string(mysql_error(sql_)));
        }

        MYSQL_RES* res = mysql_store_result(sql_);
        if (!res) {
            std::cerr << "MySQL store result error: " << mysql_error(sql_) << std::endl;
            throw std::runtime_error("MySQL store result error: " + std::string(mysql_error(sql_)));
        }

        int num_fields = mysql_num_fields(res);
        MYSQL_ROW row;

        result_type query_results;

        while ((row = mysql_fetch_row(res))) {
            std::map<std::string, std::optional<std::string>> row_map;
            for (int i = 0; i < num_fields; i++) {
                MYSQL_FIELD* field = mysql_fetch_field_direct(res, i);
                if (field) {
                    row_map[field->name] =
                        row[i] ? std::optional<std::string>(row[i]) : std::nullopt;
                }
                query_results.push_back(row_map);
            }
        }
        mysql_free_result(res);
        return query_results;
    }

private:
    MYSQL* sql_;
    SqlConnPool* pool_;
};