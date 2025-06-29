#include <chrono>

#include <boost/beast/websocket.hpp>
#include "core/http_session.hpp"
#include "spdlog/spdlog.h"

#include "core/request_handler.hpp"
#include "core/websocket_session.hpp"
#include "utils/config.hpp"

namespace websocket = boost::beast::websocket;

using WebsocketSession = tcs::core::WebsocketSession;
using AppConfig = tcs::utils::AppConfig;

namespace tcs {
namespace core {
HttpSession::HttpSession(tcp::socket socket, std::shared_ptr<std::string const> const& doc_root)
    : stream_(std::move(socket)), doc_root_(doc_root) {
    spdlog::debug("Session created on {}:{}",
                  stream_.socket().remote_endpoint().address().to_string(),
                  stream_.socket().remote_endpoint().port());
}

void HttpSession::run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&HttpSession::do_read, shared_from_this()));
}

void HttpSession::do_read() {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    parser_.emplace();
    parser_->body_limit(10000);

    // Set the timeout.
    stream_.expires_after(std::chrono::seconds(30));

    http::async_read(stream_, buffer_, *parser_,
                     beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
}

void HttpSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec == http::error::end_of_stream) {
        return do_close();
    }

    if (ec) {
        spdlog::error("Failed on_read: {}", ec.message());
        return;
    }

    if (websocket::is_upgrade(parser_->get())) {
        // HTTP IO循环结束，开始Websocket IO循环
        auto ws_session_ptr = std::make_shared<WebsocketSession>(stream_.release_socket());
        ws_session_ptr->do_accept(parser_->release());

        return;
    }

    queue_write(RequestHandler::handle_request(*doc_root_, parser_->release()));

    // 当响应数量没有达到上限时才开始下一次读取
    // 达到上限时读循环暂停
    if (response_queue_.size() < AppConfig::get().server().queue_limit()) {
        do_read();
    }
}

void HttpSession::queue_write(http::message_generator response) {
    response_queue_.push(std::move(response));
    if (response_queue_.size() == 1) {
        do_write();
    }
}

void HttpSession::do_close() {
    // Send a TCP shutdown
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
    if (ec && ec != beast::errc::not_connected) {
        spdlog::error("Failed to close HttpSession: {}", ec.message());
    }
}

void HttpSession::do_write() {
    if (!response_queue_.empty()) {
        bool keep_alive = response_queue_.front().keep_alive();
        beast::async_write(
            stream_, std::move(response_queue_.front()),
            beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), keep_alive));
    }
}

void HttpSession::send_response(http::message_generator&& msg) {
    bool keep_alive = msg.keep_alive();

    // Write the response
    beast::async_write(
        stream_, std::move(msg),
        beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), keep_alive));
}

void HttpSession::on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        spdlog::error("Failed on_write: {}", ec.message());
        return;
    }

    if (!keep_alive) {
        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        return do_close();
    }

    // 此时读循环已经暂停，且当前函数退出时，刚好队列有一个空(下方pop())，恢复读循环
    if (response_queue_.size() == AppConfig::get().server().queue_limit()) do_read();

    response_queue_.pop();

    // 写循环
    do_write();
}

}  // namespace core
}  // namespace tcs