#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <stdexcept>
#include <string>
#include <iostream>
#include <filesystem>

namespace pt = boost::property_tree;

namespace tcs {
namespace utils {
/*
* 使用前要确保已经初始化(使用init())
*/
struct AppConfig {
    // 成员变量，现在它们没有默认值，必须在构造函数中被初始化

   struct Database {
        int sqlconnpool_max_size;
        std::string server;
        std::string user;
        std::string passwd;
        std::string db;
    } database;

    struct Server {
        std::string host;
        unsigned short port;
        std::string doc_root;
        unsigned int threads;
        std::string jwt_secret;
    } server;

    void init(const std::string& filename);

    static AppConfig* getConfig() {
        static AppConfig config;
        return &config;
    }

private:
    // 核心改动：创建一个接收配置文件路径的构造函数
    explicit AppConfig() = default;
};
}  // namespace utils
}  // namespace tcs

using AppConfig = tcs::utils::AppConfig;