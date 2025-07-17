#include <mutex>

#include "core/ws_session_mgr.hpp"
#include "core/websocket_session.hpp"
#include "db/sql_conn_RAII.hpp"

namespace tcs {
namespace core {
void WSSessionMgr::broadcast(std::shared_ptr<const std::string> str_ptr) {
    std::lock_guard<std::mutex> lock(mtx_);
}
void WSSessionMgr::add_session(u64 session_id, const std::weak_ptr<WebsocketSession>& session) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        // 不存在直接创建
        sessions_.emplace(session_id, session);
    } else {
        // 如果已经存在，则更新为新的 weak_ptr
        it->second = session;
    }
}

// write to single session
void WSSessionMgr::write_to(u64 session_id, const std::string& msg) {
    std::shared_ptr<WebsocketSession> session_ptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            if (!(session_ptr = it->second.lock())) {
                sessions_.erase(it);
            }
        }
    }
    if (session_ptr) {
        session_ptr->send(std::make_shared<const std::string>(msg));
    } else {
        spdlog::warn("Session {} not found or expired", session_id);
    }
}

void WSSessionMgr::write_to_room(u64 room_id, const std::string& msg) {
    std::vector<u64> users_in_group;
    {
        SqlConnRAII conn;
        std::unique_ptr<sql::ResultSet> res(
            conn.execute_query("SELECT user_uuid FROM room_members WHERE room_uuid = ?", room_id));

        while (res->next()) {
            u64 user_id = res->getUInt64("user_id");
            users_in_group.push_back(user_id);
        }
    }

    std::vector<std::shared_ptr<WebsocketSession>> online_users;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& user_id : users_in_group) {
            auto it = sessions_.find(user_id);
            if (it != sessions_.end()) {
                if (auto session_ptr = it->second.lock()) {
                    online_users.push_back(session_ptr);
                } else {
                    sessions_.erase(it);  // 清理过期会话
                }
            }
        }
    }

    for (const auto& session_ptr : online_users) {
        session_ptr->send(std::make_shared<const std::string>(msg));
    }
}

}  // namespace core
}  // namespace tcs