#include "db/sql_conn_pool.hpp"

#include <stdexcept>
#include <memory>
#include <mutex>

using AppConfig = tcs::utils::AppConfig;

namespace tcs {
namespace db {

SqlConnPool* SqlConnPool::instance() {
    static SqlConnPool pool;
    return &pool;
}

/*
 * 获取连接
 * 会进行健康检查，如果连接不可用则重新初始化连接
 */
Connection* SqlConnPool::getConn() {
    if (smph_->try_acquire()) {
        return getSql();
    }
    // 通过日志判断连接池负载
    std::cout << "Sql connect pool is busy..." << std::endl;

    smph_->acquire();
    return getSql();
}

void SqlConnPool::init() {
    max_conn_ = AppConfig::get().database().sqlconnpool_max_size();
    smph_ = std::make_unique<std::counting_semaphore<SEMAPHORE_MAX_VALUE>>(
        AppConfig::get().database().sqlconnpool_max_size());
    MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();

    sql::ConnectOptionsMap connection_properties;
    connection_properties["hostName"] = AppConfig::get().database().server();
    connection_properties["userName"] = AppConfig::get().database().user();
    connection_properties["password"] = AppConfig::get().database().passwd();
    connection_properties["schema"] = AppConfig::get().database().db();
    connection_properties["OPT_RECONNECT"] = true;  // 关键选项

    // #ifdef MYSQL_PLUGIN_DIR
    //     connection_properties["lib_extra_options"] = "plugin_dir=" MYSQL_PLUGIN_DIR;
    //     std::cout << "Using MySQL Plugin Dir: " << MYSQL_PLUGIN_DIR << std::endl;
    // #endif

    if (!driver) {
        std::cerr << "MySQL driver instance is null!" << std::endl;
        throw std::runtime_error("MySQL driver instance is null!");
    }

    for (int i{}; i < max_conn_; i++) {
        Connection* sql(driver->connect(connection_properties));
        conn_queue_.push(sql);
    }
}

void SqlConnPool::freeConn(Connection* sql) {
    // 检查连接是否有效
    if (sql) {
        std::lock_guard lock(mtx_);
        conn_queue_.push(sql);
        smph_->release();
    }
}

void SqlConnPool::closePool() {
    std::lock_guard<std::mutex> lock(mtx_);
    while (!conn_queue_.empty()) {
        auto sql = conn_queue_.front();
        sql->close();
        conn_queue_.pop();
    }
}

SqlConnPool::~SqlConnPool() { closePool(); }

/*
等待测试--失效连接重新激活
*/
Connection* SqlConnPool::getSql() {
    Connection* sql = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        // 如果队列空了，这通常表示一个逻辑错误，因为信号量保证了有连接可用
        // 但作为健壮性检查，可以加上
        if (conn_queue_.empty()) {
            // 这种情况理论上不该发生，因为 acquire() 成功了
            // 可以选择抛出异常或创建一个新连接
            std::cerr << "CRITICAL: Semaphore acquired but connection queue is empty!" << std::endl;
            // 这里我们选择抛出异常，因为它指示了一个严重的同步问题
            throw std::runtime_error("Connection pool synchronization error.");
        }
        sql = conn_queue_.front();
        conn_queue_.pop();
    }
    if (sql->isValid()) {
        return sql;
    } else {
        // 如果连接失效，重新创建一个新的连接
        std::cerr << "Connection is invalid, creating a new one." << std::endl;
        MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        if (!driver) {
            throw std::runtime_error("MySQL driver instance is null!");
        }
        sql = driver->connect(AppConfig::get().database().server(),
                              AppConfig::get().database().user(),
                              AppConfig::get().database().passwd());
        return sql;
    }
}

SqlConnPool::SqlConnPool() : smph_(nullptr), max_conn_(0) {}
}  // namespace db
}  // namespace tcs