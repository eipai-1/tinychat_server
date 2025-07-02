#include <chrono>
#include <memory>

#include <boost/beast/websocket.hpp>
#include "core/http_session.hpp"
#include "spdlog/spdlog.h"

#include "core/request_handler.hpp"
#include "core/websocket_session.hpp"
#include "pool/thread_pool.hpp"
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

    // 已满 不读
    if (inflight_requests_ >= AppConfig::get().server().queue_limit()) {
        spdlog::warn("Http requests reached limited! Read paused.");
        return;
    }

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

    ++inflight_requests_;

    auto req_ptr = std::make_shared<http::request<http::string_body>>(parser_->release());

    // 2. 将“处理这个请求”作为一个任务，提交给工作线程池。
    //    我们使用 lambda 来捕获所有需要的信息。
    pool::ThreadPool::get().addTask([this, self = shared_from_this(), req_ptr] {
        http::message_generator response =
            RequestHandler::handle_request(*doc_root_, std::move(*req_ptr));

        // c. 【关键】业务处理完成，但我们不能在这里直接发送响应！
        //    因为网络写操作必须在属于这个 session 的 I/O 线程 (strand) 上执行。
        //    所以，我们需要把生成的响应再“投递”回 I/O 线程。
        this->queue_response_from_worker(std::move(response));
    });
}

void HttpSession::queue_write(http::message_generator&& response) {
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

void HttpSession::queue_response_from_worker(http::message_generator&& response) {
    net::post(
        stream_.get_executor(),
        beast::bind_front_handler(&HttpSession::queue_write,  // 调用我们已有的 queue_write 方法
                                  shared_from_this(), std::move(response)));
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

    --inflight_requests_;

    // 此时读循环已经暂停，且当前函数退出时，刚好队列有一个空(下方pop())，恢复读循环
    if (response_queue_.size() == AppConfig::get().server().queue_limit()) {
        do_read();
    }

    response_queue_.pop();

    // 写循环
    do_write();
}

}  // namespace core
}  // namespace tcs