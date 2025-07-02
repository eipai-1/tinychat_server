#include "utils/config.hpp"

namespace tcs {
namespace utils {
std::unique_ptr<AppConfig> AppConfig::instance_ptr_ = nullptr;

void AppConfig::init(const std::string& filename) {
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
        instance_ptr_.reset(new AppConfig);
        // 3. 逐个读取配置项，并直接赋值给成员变量
        //    使用一个辅助 lambda 来避免重复代码
        auto get_value = [&](const std::string& path) {
            return config_tree.get_optional<std::string>(path);
        };

        instance_ptr_->database_.sqlconnpool_max_size(
            config_tree.get<int>("Database.sqlconnpool_max_size"));
        instance_ptr_->database_.server(config_tree.get<std::string>("Database.server"));
        instance_ptr_->database_.user(config_tree.get<std::string>("Database.user"));
        instance_ptr_->database_.passwd(config_tree.get<std::string>("Database.passwd"));
        instance_ptr_->database_.db(config_tree.get<std::string>("Database.db"));

        instance_ptr_->server_.host(config_tree.get<std::string>("Server.host"));
        instance_ptr_->server_.port(config_tree.get<unsigned short>("Server.port"));
        instance_ptr_->server_.doc_root(config_tree.get<std::string>("Server.doc_root"));
        instance_ptr_->server_.threads(config_tree.get<unsigned int>("Server.threads"));
        instance_ptr_->server_.jwt_secret(config_tree.get<std::string>("Server.jwt_secret"));
        instance_ptr_->server_.log_file(config_tree.get<std::string>("Server.log_file"));
        instance_ptr_->server_.queue_limit(config_tree.get<unsigned int>("Server.queue_limit"));

    } catch (const pt::ptree_error& e) {
        // 捕获所有 property_tree 相关的错误
        throw std::runtime_error("Invalid configuration in '" + filename +
                                 "'. Reason: " + e.what());
    }

    std::cout << "Configuration successfully loaded from " << filename << std::endl;
}
}  // namespace utils
}  // namespace tcs