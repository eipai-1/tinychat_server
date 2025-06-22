#include <chrono>

#include "core/session.hpp"
#include "spdlog/spdlog.h"
#include "core/request_handler.hpp"

namespace tcs {
namespace core {
Session::Session(tcp::socket socket, std::shared_ptr<std::string const> const& doc_root)
    : stream_(std::move(socket)), doc_root_(doc_root) {
    spdlog::debug("Session created on {}:{}",
                  stream_.socket().remote_endpoint().address().to_string(),
                  stream_.socket().remote_endpoint().port());
}

void Session::run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&Session::do_read, shared_from_this()));
}

void Session::do_read() {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req_ = {};

    // Set the timeout.
    stream_.expires_after(std::chrono::seconds(30));

    http::async_read(stream_, buffer_, req_,
                     beast::bind_front_handler(&Session::on_read, shared_from_this()));
}

void Session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec == http::error::end_of_stream) {
        return do_close();
    }

    if (ec) {
        spdlog::error("Failed on_read: {}", ec.message());
        return;
    }

    send_response(RequestHandler::handle_request(
        *doc_root_, std::move(req_)));  // Process the request and send response
}

void Session::do_close() {
    // Send a TCP shutdown
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
    if (ec && ec != beast::errc::not_connected) {
        spdlog::error("Failed to close session: {}", ec.message());
    }
}

void Session::send_response(http::message_generator&& msg) {
    bool keep_alive = msg.keep_alive();

    // Write the response
    beast::async_write(
        stream_, std::move(msg),
        beast::bind_front_handler(&Session::on_write, shared_from_this(), keep_alive));
}

void Session::on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred) {
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

    // Read another request
    do_read();
}

}  // namespace core
}  // namespace tcs