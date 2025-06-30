#pragma once

#include "utils/net_utils.hpp"
#include "utils/config.hpp"
#include "db/sql_conn_pool.hpp"
#include "pool/thread_pool.hpp"
#include "core/listener.hpp"
#include "core/ws_session_mgr.hpp"

namespace tcs {
class TinychatServer {
public:
    explicit TinychatServer();
    void run();
    void init_log();
    ~TinychatServer();

private:
    net::io_context ioc_;
    std::shared_ptr<core::Listener> listener_;
};
}  // namespace tcs