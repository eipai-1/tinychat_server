#pragma once

#include "utils/net_utils.hpp"
#include <memory>

namespace tcs {
namespace core {
class Listener : public std::enable_shared_from_this<Listener> {
public:
    explicit Listener(net::io_context& ioc, tcp::endpoint endpoint, const std::string& doc_root);

    // Start accepting incoming connections
    void run();

private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    const std::string doc_root_;
};
}  // namespace core
}  // namespace tcs