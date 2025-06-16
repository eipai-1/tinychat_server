#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <stdexcept>
#include <string>
#include <iostream>
#include <filesystem>

namespace pt = boost::property_tree;

struct AppConfig {
    // 成员变量，现在它们没有默认值，必须在构造函数中被初始化

    struct Database {
        int sqlconnpool_max_size;
        std::string host;
        std::string user;
        std::string passwd;
        std::string db;
        unsigned int port;
    } database;

    // struct Server {
    //     std::string host;
    // } server;

    // 核心改动：创建一个接收配置文件路径的构造函数
    explicit AppConfig(const std::string& filename) {
        // 1. 检查文件是否存在
        if (!std::filesystem::exists(filename)) {
            throw std::runtime_error("Configuration file not found: " + filename);
        }

        pt::ptree config_tree;
        try {
            // 2. 解析 INI 文件
            pt::read_ini(filename, config_tree);
        } catch (const pt::ini_parser_error& e) {
            throw std::runtime_error("Failed to parse INI file '" + filename +
                                     "'. Reason: " + e.what());
        }

        try {
            // 3. 逐个读取配置项，并直接赋值给成员变量
            //    使用一个辅助 lambda 来避免重复代码
            auto get_value = [&](const std::string& path) {
                return config_tree.get_optional<std::string>(path);
            };

            // --- 读取并校验数据库连接池配置 ---
            // 使用 get_optional 配合 or_else 可以提供更灵活的错误处理
            // 但为了严格性，我们还是用 get 来强制要求所有字段必须存在
            this->database.sqlconnpool_max_size =
                config_tree.get<int>("Database.sqlconnpool_max_size");
            if (this->database.sqlconnpool_max_size <= 0) {
                throw std::runtime_error(
                    "Value for 'sqlconnpool_max_size' must be a positive integer.");
            }

            // --- 读取并校验数据库连接信息 ---
            this->database.host = config_tree.get<std::string>("Database.host");
            if (this->database.host.empty()) {
                throw std::runtime_error("Value for 'host' cannot be empty.");
            }

            this->database.user = config_tree.get<std::string>("Database.user");
            if (this->database.user.empty()) {
                throw std::runtime_error("Value for 'user' cannot be empty.");
            }

            this->database.passwd = config_tree.get<std::string>("Database.passwd");

            this->database.db = config_tree.get<std::string>("Database.db");
            if (this->database.db.empty()) {
                throw std::runtime_error("Value for 'db' (database name) cannot be empty.");
            }

            this->database.port = config_tree.get<unsigned int>("Database.port");
            if (this->database.port == 0 || this->database.port > 65535) {
                throw std::runtime_error("Value for 'port' must be between 1 and 65535.");
            }

        } catch (const pt::ptree_error& e) {
            // 捕获所有 property_tree 相关的错误
            throw std::runtime_error("Invalid configuration in '" + filename +
                                     "'. Reason: " + e.what());
        }

        std::cout << "Configuration successfully loaded from " << filename << std::endl;
    }
};