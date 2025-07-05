#include "tinychat_server.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
#ifdef NDEBUG
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }
#endif  //  NDEBUG

    try {
        // 加载配置文件
#ifdef NDEBUG
        tcs::utils::AppConfig::init(argv[1]);
#else
        tcs::utils::AppConfig::init("../../doc/config.ini");
#endif

        // 初始化服务器
        tcs::TinychatServer server;
        // 启动服务器
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}