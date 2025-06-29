#include <mutex>

#include "core/ws_session_mgr.hpp"
#include "core/websocket_session.hpp"

namespace tcs {
namespace core {
void WSSessionMgr::broadcast(std::shared_ptr<const std::string> str_ptr) {
    std::lock_guard<std::mutex> lock(mtx_);
}
void WSSessionMgr::add_session(const std::string& session_uuid,
                               const std::weak_ptr<WebsocketSession>& session) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = sessions_.find(session_uuid);
    if (it == sessions_.end()) {
        // 不存在直接创建
        sessions_.emplace(session_uuid, session);
    } else {
        // 如果已经存在，则更新为新的 weak_ptr
        it->second = session;
    }
}

// write to single session
void WSSessionMgr::write_to(const std::string& session_uuid, const std::string& msg) {
    std::shared_ptr<WebsocketSession> session_ptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = sessions_.find(session_uuid);
        if (it != sessions_.end()) {
            if (!(session_ptr = it->second.lock())) {
                sessions_.erase(it);
            }
        }
    }
    if (session_ptr) {
        session_ptr->send(std::make_shared<const std::string>(msg));
    } else {
        spdlog::warn("Session {} not found or expired", session_uuid);
    }
}
}  // namespace core
}  // namespace tcs