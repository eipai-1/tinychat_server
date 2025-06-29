#pragma once

#include "utils/net_utils.hpp"
#include <optional>
#include <boost/json.hpp>
#include <memory>
#include <queue>

namespace tcs {
namespace core {
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    explicit HttpSession(tcp::socket socket, std::shared_ptr<std::string const> const& doc_root);
    void run();

private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    std::queue<http::message_generator> response_queue_;

    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void queue_write(http::message_generator response);
    void do_close();
    void do_write();
    void on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred);
    void send_response(http::message_generator&& msg);
};
}  // namespace core
}  // namespace tcs
