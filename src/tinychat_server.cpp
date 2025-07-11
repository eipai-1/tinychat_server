#include <thread>
#include <memory>
#include <boost/asio/signal_set.hpp>
#include <sodium.h>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/async.h"

#include "tinychat_server.hpp"
#include "utils/config.hpp"
#include "db/sql_conn_pool.hpp"
#include "utils/net_utils.hpp"

using AppConfig = tcs::utils::AppConfig;

namespace tcs {
void TinychatServer::init_log() {
    // 初始化日志系统
    spdlog::init_thread_pool(8192, 1);
    auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
        AppConfig::get().server().log_file(), 0, 0, true);

#ifndef NDEBUG
    daily_sink->set_level(spdlog::level::debug);
#else
    daily_sink->set_level(spdlog::level::info);
#endif
    // auto logger = std::make_shared<spdlog::logger>("tinychat_server", daily_sink);
    auto async_logger = spdlog::create_async<spdlog::sinks::daily_file_sink_mt>(
        "daily_logger", AppConfig::get().server().log_file(), 0, 0);

#ifndef NDEBUG
    async_logger->set_level(spdlog::level::debug);
#else
    async_logger->set_level(spdlog::level::err);
#endif

    spdlog::set_default_logger(async_logger);

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");

    spdlog::flush_every(std::chrono::seconds(5));

#ifndef NDEBUG
    spdlog::flush_on(spdlog::level::debug);
#else
    spdlog::flush_on(spdlog::level::err);
#endif
    spdlog::info("----Log initialized successfully----");
}

TinychatServer::TinychatServer()
    : ioc_(AppConfig::get().server().io_threads()), listener_(nullptr) {
    // 初始化日志
    init_log();
    sodium_init();

    db::SqlConnPool::instance()->init();

    pool::ThreadPool::init(AppConfig::get().server().worker_threads());

    listener_ = std::make_shared<core::Listener>(
        ioc_,
        tcp::endpoint(net::ip::make_address(AppConfig::get().server().host()),
                      AppConfig::get().server().port()),
        AppConfig::get().server().doc_root());

    // 初始化
    tcs::core::WSSessionMgr::get();

    spdlog::info("Tinychat server started successfully on {}:{}. Document root: {}",
                 AppConfig::get().server().host(), AppConfig::get().server().port(),
                 AppConfig::get().server().doc_root());
}

void TinychatServer::run() {
    // 启动 I/O 上下文
    net::signal_set signals(ioc_, SIGINT, SIGTERM);
    // SIGTERM 是另一种常见的终止信号，比如 `kill` 命令默认发送的信号

    // 2. 发起一个异步等待操作
    //    当收到 SIGINT 或 SIGTERM 时，执行提供的 Lambda 回调函数
    signals.async_wait([&](const beast::error_code& error, int signal_number) {
        // 这个 Lambda 会在 ioc 的某个线程上被执行
        spdlog::warn("Shutdown signal received (Signal number: {}).", signal_number);

        // 关键一步：优雅地停止 io_context
        // io_context::stop() 会导致所有阻塞在 ioc.run() 上的线程立即返回。
        ioc_.stop();
    });
    // ----------------------------
    listener_->run();
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < AppConfig::get().server().io_threads(); ++i) {
        threads.emplace_back([this]() { ioc_.run(); });
    }
    ioc_.run();
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

TinychatServer::~TinychatServer() {
    spdlog::info("Tinychat server is shutting down...");
    spdlog::default_logger()->flush();
    spdlog::shutdown();
}
}  // namespace tcs