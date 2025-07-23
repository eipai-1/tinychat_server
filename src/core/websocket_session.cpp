#include <memory>

#include "core/websocket_session.hpp"
#include "core/ws_session_mgr.hpp"
#include "utils/net_utils.hpp"
#include "core/ws_handler.hpp"
#include "db/sql_conn_RAII.hpp"
#include "model/auth_models.hpp"
#include "pool/thread_pool.hpp"

using UserClaims = tcs::model::UserClaims;
using SqlConnRAII = tcs::db::SqlConnRAII;

namespace tcs {
namespace core {
void WebsocketSession::auth_user(const std::string& token) {
    user_claims_ = RequestHandler::extract_user_claims(token);

    WSSessionMgr::get().add_session(user_claims_.id, shared_from_this());

    spdlog::debug("WebsocketSession: User {} authenticated with UUID: {}", user_claims_.username,
                  user_claims_.id);
}

void WebsocketSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec == websocket::error::closed) {
        return;
    }

    if (ec) {
        spdlog::error("Websocket Session on_read failed:" + ec.message());
        return;
    }

    // todo: 流量控制

    pool::ThreadPool::get().addTask(WSHandler::handle_message,
                                    beast::buffers_to_string(buffer_.data()), user_claims_);
    // WSHandler::handle_message(beast::buffers_to_string(buffer_.data()), user_id_);

    do_read();
}

void WebsocketSession::on_send(const std::shared_ptr<const std::string>& str_ptr) {
    message_queue_.emplace(str_ptr);

    if (message_queue_.size() == 1) {
        ws_.async_write(net::buffer(*message_queue_.front()),
                        beast::bind_front_handler(&WebsocketSession::on_write, shared_from_this()));
    }
}

void WebsocketSession::on_write(beast::error_code ec, std::size_t bytes_transferred) {
    // boost::ignore_unused(bytes_transferred);

    if (ec) {
        spdlog::error("Websocket Session on_write failed:" + ec.message());
        return;
    }

    message_queue_.pop();

    if (!message_queue_.empty()) {
        ws_.async_write(net::buffer(*message_queue_.front()),
                        beast::bind_front_handler(&WebsocketSession::on_write, shared_from_this()));
    }
}
}  // namespace core
}  // namespace tcs