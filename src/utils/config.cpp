#include "utils/config.hpp"

namespace tcs {
namespace utils {
std::atomic<AppConfig*> AppConfig::instance_ptr_(nullptr);
std::once_flag AppConfig::init_flag_;

void AppConfig::init(const std::string& filename) {
    std::call_once(init_flag_, [&]() {
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
            auto ptr = new AppConfig();
            // 3. 逐个读取配置项，并直接赋值给成员变量
            //    使用一个辅助 lambda 来避免重复代码
            auto get_value = [&](const std::string& path) {
                return config_tree.get_optional<std::string>(path);
            };
            Server server;
            Database database;

            server.host(config_tree.get<std::string>("Server.host"));
            server.port(config_tree.get<unsigned short>("Server.port"));
#ifdef PLATFORM_WINDOWS
            server.doc_root("D:\\program\\cpp\\proj\\tinychat_server\\doc\\assets\\");
#else
            server.doc_root(config_tree.get<std::string>("Server.doc_root"));
#endif
            server.io_threads(config_tree.get<unsigned int>("Server.io_threads"));
            server.worker_threads(config_tree.get<unsigned int>("Server.worker_threads"));
            server.jwt_secret(config_tree.get<std::string>("Server.jwt_secret"));
            server.log_file(config_tree.get<std::string>("Server.log_file"));
            server.queue_limit(config_tree.get<unsigned int>("Server.queue_limit"));
            server.custom_epoch(config_tree.get<u64>("Server.custom_epoch"));
            server.service_id(config_tree.get<u64>("Server.service_id"));

            database.sqlconnpool_max_size(config_tree.get<int>("Database.sqlconnpool_max_size"));
            database.server(config_tree.get<std::string>("Database.server"));
            database.user(config_tree.get<std::string>("Database.user"));
            database.passwd(config_tree.get<std::string>("Database.passwd"));
            database.db(config_tree.get<std::string>("Database.db"));
            ptr->database_ = database;
            ptr->server_ = server;
            instance_ptr_.store(ptr, std::memory_order_release);

        } catch (const pt::ptree_error& e) {
            // 捕获所有 property_tree 相关的错误
            throw std::runtime_error("Invalid configuration in '" + filename +
                                     "'. Reason: " + e.what());
        }

        std::cout << "Configuration successfully loaded from " << filename << std::endl;
    });
}

}  // namespace utils
}  // namespace tcs