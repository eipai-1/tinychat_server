#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <stdexcept>
#include <string>
#include <iostream>
#include <filesystem>
#include <memory>

namespace pt = boost::property_tree;

namespace tcs {
namespace utils {
class AppConfig {
    // 成员变量，现在它们没有默认值，必须在构造函数中被初始化
public:
    class Database {
    public:
        void sqlconnpool_max_size(int size) {
            if (size <= 0) {
                throw std::invalid_argument("sqlconnpool_max_size must be a positive integer.");
            }
            sqlconnpool_max_size_ = size;
        }
        void server(const std::string& server) {
            if (server.empty()) {
                throw std::invalid_argument("Database server cannot be empty.");
            }
            server_ = server;
        }
        void user(const std::string& user) {
            if (user.empty()) {
                throw std::invalid_argument("Database user cannot be empty.");
            }
            user_ = user;
        }
        void passwd(const std::string& passwd) {
            if (passwd.empty()) {
                throw std::invalid_argument("Database password cannot be empty.");
            }
            passwd_ = passwd;
        }
        void db(const std::string& db) {
            if (db.empty()) {
                throw std::invalid_argument("Database name cannot be empty.");
            }
            db_ = db;
        }

        int sqlconnpool_max_size() const { return sqlconnpool_max_size_; }
        const std::string& server() const { return server_; }
        const std::string& user() const { return user_; }
        const std::string& passwd() const { return passwd_; }
        const std::string& db() const { return db_; }

    private:
        // 数据库连接池的最大连接数
        int sqlconnpool_max_size_ = 0;
        // 数据库服务器
        std::string server_;
        // 数据库用户
        std::string user_;
        // 数据库密码
        std::string passwd_;
        // 数据库名称
        std::string db_;
    };

    class Server {
    public:
        void host(const std::string& host) {
            if (host.empty()) {
                throw std::invalid_argument("Server host cannot be empty.");
            }
            host_ = host;
        }
        void port(unsigned short port) {
            if (port == 0) {
                throw std::invalid_argument("Server port must be a non-zero value.");
            }
            port_ = port;
        }
        void doc_root(const std::string& doc_root) {
            if (doc_root.empty()) {
                throw std::invalid_argument("Document root cannot be empty.");
            }
            doc_root_ = doc_root;
        }
        void io_threads(unsigned int threads) {
            if (threads == 0) {
                throw std::invalid_argument("IO threads must be a positive integer.");
            }
            io_threads_ = threads;
        }
        void worker_threads(unsigned int threads) {
            if (threads == 0) {
                throw std::invalid_argument("Worker threads must be a positive integer.");
            }
            worker_threads_ = threads;
        }
        void jwt_secret(const std::string& jwt_secret) {
            if (jwt_secret.empty()) {
                throw std::invalid_argument("JWT secret cannot be empty.");
            }
            jwt_secret_ = jwt_secret;
        }
        void log_file(const std::string& log_file) {
            if (log_file.empty()) {
                throw std::invalid_argument("Log file path cannot be empty.");
            }
            log_file_ = log_file;
        }
        void queue_limit(unsigned int limit) {
            if (limit == 0) {
                throw std::invalid_argument("Queue limit must be a positive integer.");
            }
            queue_limit_ = limit;
        }

        const std::string& host() const { return host_; }
        unsigned short port() const { return port_; }
        const std::string& doc_root() const { return doc_root_; }
        unsigned int io_threads() const { return io_threads_; }
        unsigned int worker_threads() const { return worker_threads_; }
        const std::string& jwt_secret() const { return jwt_secret_; }
        const std::string& log_file() const { return log_file_; }
        unsigned int queue_limit() const { return queue_limit_; }

    private:
        // 服务器监听地址
        std::string host_;
        // 服务器监听端口
        unsigned short port_ = 0;
        // 服务器文档根目录
        std::string doc_root_;
        // ioc的线程数
        unsigned int io_threads_ = 0;
        // 线程池线程数
        unsigned int worker_threads_ = 0;
        // jwt密钥
        std::string jwt_secret_;
        // 日志文件路径
        std::string log_file_;
        unsigned int queue_limit_ = 0;
    };

    static void init(const std::string& filename);

    static const AppConfig& get() {
        if (!instance_ptr_) {
            throw std::runtime_error("AppConfig has not been initialized. Call init() first.");
        }
        return *instance_ptr_;
    }

    const Server& server() const { return server_; }
    const Database& database() const { return database_; }

private:
    // 核心改动：创建一个接收配置文件路径的构造函数
    explicit AppConfig() = default;
    Server server_;
    Database database_;
    static std::unique_ptr<AppConfig> instance_ptr_;
};
}  // namespace utils
}  // namespace tcs
