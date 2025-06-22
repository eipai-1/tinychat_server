#pragma once

#include "utils/net_utils.hpp"
#include <boost/json.hpp>
#include <memory>

namespace tcs {
namespace core {
class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket, std::shared_ptr<std::string const> const& doc_root);
    void run();

private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    http::request<http::string_body> req_;

    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void do_write();
    void do_close();
    void on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred);
    void send_response(http::message_generator&& msg);
};
}  // namespace core
}  // namespace tcs
