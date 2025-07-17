#pragma once

#include <memory>
#include <boost/beast/websocket.hpp>
#include <spdlog/spdlog.h>
#include <queue>
#include <string>

#include "utils/net_utils.hpp"
#include "utils/types.hpp"
#include "core/request_handler.hpp"
#include "core/ws_session_mgr.hpp"
#include "model/auth_models.hpp"

namespace websocket = boost::beast::websocket;

using UserClaims = tcs::model::UserClaims;

namespace tcs {
namespace core {
class WebsocketSession : public std::enable_shared_from_this<WebsocketSession> {
public:
    explicit WebsocketSession(tcp::socket&& socket) : ws_(std::move(socket)) {
        spdlog::debug("WebsocketSession created on {}:{}",
                      ws_.next_layer().socket().remote_endpoint().address().to_string(),
                      ws_.next_layer().socket().remote_endpoint().port());
    }

    template <typename Body, typename Allocator>
    void do_accept(http::request<Body, http::basic_fields<Allocator>> req) {
        UserClaims user_claims;
        try {
            if (req.find(http::field::authorization) == req.end()) {
                spdlog::warn("WebsocketSession: Authorization header not found");
                throw std::runtime_error("Authorization header not found");
            }
            std::string token = std::string(req[http::field::authorization]);
            if (token.empty()) {
                spdlog::warn("WebsocketSession: Empty authorization token");
                throw std::runtime_error("Empty authorization token");
            }
            auth_user(token);
        } catch (const std::exception& e) {
            spdlog::error("WebsocketSession: Failed to handle request: {}", e.what());
            return;
        }

        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
            res.set(http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) + " tinychat_server");
        }));
        ws_.async_accept(
            req, beast::bind_front_handler(&WebsocketSession::on_accept, shared_from_this()));
    }

    // critical
    void send(const std::shared_ptr<const std::string>& str_ptr) {
        net::post(ws_.get_executor(), beast::bind_front_handler(&WebsocketSession::on_send,
                                                                shared_from_this(), str_ptr));
    }

    ~WebsocketSession() {
        spdlog::debug("WebsocketSession for user {} is being destroyed.", user_claims_.username);
        // 清理会话
        WSSessionMgr::get().remove_session(user_claims_.id);
    };

private:
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::queue<std::shared_ptr<const std::string>> message_queue_;
    UserClaims user_claims_;

    void auth_user(const std::string& token);

    void on_accept(beast::error_code ec) {
        if (ec) {
            spdlog::error("WebsocketSession: Failed on accept:" + ec.message());
            return;
        }

        do_read();
    }

    void do_read() {
        ws_.async_read(buffer_,
                       beast::bind_front_handler(&WebsocketSession::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred);

    void on_send(const std::shared_ptr<const std::string>& str_ptr);

    void on_write(beast::error_code ec, std::size_t bytes_transferred);
};
}  // namespace core
}  // namespace tcs