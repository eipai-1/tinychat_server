#include <stdexcept>

#include <boost/json.hpp>
#include <spdlog/spdlog.h>

#include "model/ws_models.hpp"
#include "core/request_handler.hpp"
#include "core/ws_handler.hpp"
#include "core/ws_session_mgr.hpp"
#include "db/sql_conn_RAII.hpp"
#include "utils/enums.hpp"

namespace model = tcs::model;
namespace json = boost::json;
namespace utils = tcs::utils;

using SqlConnRAII = tcs::db::SqlConnRAII;

namespace tcs {
namespace core {
void WSHandler::handle_message(const std::string& msg_ptr, const std::string& user_uuid) {
    try {
        json::value jv = json::parse(msg_ptr);

        if (!jv.is_object()) {
            throw std::runtime_error("Invalid JSON format in WebSocket message");
            return;
        }
        const json::object obj = jv.as_object();

        if (!obj.contains("type") || !obj.at("type").is_string()) {
            throw std::runtime_error("Missing or invalid 'type' field in WebSocket message");
        }

        std::string type = obj.at("type").as_string().c_str();

        SqlConnRAII conn;
        if (type == "private_message") {
            // todo: 好友检测
            model::ClientPrivateMsg private_msg =
                json::value_to<model::ClientPrivateMsg>(jv.at("data"));
            std::string msg_uuid = RequestHandler::generate_uuid();
            std::string room_uuid =
                RequestHandler::generate_private_room_uuid(user_uuid, private_msg.other_user_uuid);

            int update_row = conn.execute_update(
                "INSERT INTO messages (uuid, room_uuid, sender_uuid, content) VALUES (?, ?, ?, ?)",
                msg_uuid, room_uuid, user_uuid, private_msg.content);
            if (update_row != 1) {
                spdlog::error("Failed to insert private message into database for user: {}",
                              user_uuid);
                throw std::runtime_error("Failed to insert private message");
            }

            spdlog::debug("Inserted messages {}", msg_uuid);

            model::ServerRespMsg<model::PrivateMsgToSend> private_msg_to_send = {
                .type = utils::ServerRespType::PMsgToSend,
                .data = model::PrivateMsgToSend{.sender_uuid = user_uuid,
                                                .content = private_msg.content}};

            model::ServerRespMsg<std::string> msg_sent_info = {
                .type = utils::ServerRespType::MsgSentInfo, .data = std::string("")};

            WSSessionMgr::get().write_to(private_msg.other_user_uuid,
                                         json::serialize(json::value_from(private_msg_to_send)));

            WSSessionMgr::get().write_to(user_uuid,
                                         json::serialize(json::value_from(msg_sent_info)));

        } else if (type == "group_message") {
            model::ClientGroupMsg group_msg = json::value_to<model::ClientGroupMsg>(jv.at("data"));

            std::string msg_uuid = RequestHandler::generate_uuid();
            int update_row = conn.execute_update(
                "INSERT INTO messages (uuid, room_uuid, sender_uuid, content) VALUES (?, ?, ?, ?)",
                msg_uuid, group_msg.room_uuid, user_uuid, group_msg.content);

            if (update_row != 1) {
                spdlog::error("Failed to insert group message into database for user: {}",
                              user_uuid);
                throw std::runtime_error("Failed to insert group message");
            }

            spdlog::debug("Inserted group message {}", msg_uuid);

            model::ServerRespMsg<model::GroupMsgToSend> group_msg_to_send = {
                .type = utils::ServerRespType::GMsgToSend,
                .data = model::GroupMsgToSend{.room_uuid = group_msg.room_uuid,
                                              .sender_uuid = user_uuid,
                                              .content = group_msg.content}};

            WSSessionMgr::get().write_to_room(group_msg.room_uuid,
                                              json::serialize(json::value_from(group_msg_to_send)));
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle websocket message:{}", e.what());
    }
}
}  // namespace core
}  // namespace tcs
