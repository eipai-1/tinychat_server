#pragma once

#include <memory>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <map>
#include <string>
#include <string_view>

// #include "core/websocket_session.hpp" //circular denpendency
#include "utils/types.hpp"

namespace tcs {
namespace core {
class WebsocketSession;  // Forward declaration to avoid circular dependency

class WSSessionMgr {
public:
    static WSSessionMgr& get() {
        static WSSessionMgr instance;
        return instance;
    }

    void broadcast(std::shared_ptr<const std::string> str_ptr);

    void add_session(u64 session_id, const std::weak_ptr<WebsocketSession>& session);

    void remove_session(u64 session_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            sessions_.erase(it);
        }
    }

    // Write to a single session
    void write_to(u64 session_id, const std::string& msg);

    void write_to_room(u64 room_id, const std::string& msg);

private:
    WSSessionMgr() {}

    std::mutex mtx_;

    std::unordered_map<u64, std::weak_ptr<WebsocketSession>> sessions_;
};
}  // namespace core
}  // namespace tcs