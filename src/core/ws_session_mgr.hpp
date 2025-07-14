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

    void add_session(const std::string& session_uuid,
                     const std::weak_ptr<WebsocketSession>& session);

    void remove_session(const std::string& session_uuid) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = sessions_.find(session_uuid);
        if (it != sessions_.end()) {
            sessions_.erase(it);
        }
    }

    // Write to a single session
    void write_to(const std::string& session_uuid, const std::string& msg);

    void write_to_room(const std::string& room_uuid, const std::string& msg);

private:
    WSSessionMgr() {}

    std::mutex mtx_;

    std::unordered_map<std::string, std::weak_ptr<WebsocketSession>> sessions_;
};
}  // namespace core
}  // namespace tcs