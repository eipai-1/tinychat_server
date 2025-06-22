#include "core/listener.hpp"
#include "spdlog/spdlog.h"
#include "core/session.hpp"

namespace tcs {
namespace core {
Listener::Listener(net::io_context& ioc, tcp::endpoint endpoint, const std::string& doc_root)
    : ioc_(ioc), acceptor_(net::make_strand(ioc)), doc_root_(doc_root) {
    beast::error_code ec;

    // Open the acceptor
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        spdlog::error("Failed to open acceptor: {}", ec.message());
        return;
    }

    // Allow address reuse
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        spdlog::error("Failed to set option: {}", ec.message());
        return;
    }

    // Bind to the server address
    acceptor_.bind(endpoint, ec);
    if (ec) {
        spdlog::error("Failed to bind acceptor: {}", ec.message());
        return;
    }

    // Start listening for connections
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        spdlog::error("Failed to listen on acceptor: {}", ec.message());
        return;
    }
}

void Listener::run() { do_accept(); }

void Listener::do_accept() {
    // The new connection gets its own strand
    acceptor_.async_accept(net::make_strand(ioc_),
                           beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
}

void Listener::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        spdlog::error("Failed to accept connection: {}", ec.message());
        return;
    } else {
        std::make_shared<Session>(std::move(socket), std::make_shared<const std::string>(doc_root_))
            ->run();
    }
    do_accept();  // Accept another connection
}

}  // namespace core
}  // namespace tcs