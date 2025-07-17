#include <stdexcept>

#include <boost/json.hpp>
#include <spdlog/spdlog.h>

#include "model/ws_models.hpp"
#include "core/request_handler.hpp"
#include "core/ws_handler.hpp"
#include "core/ws_session_mgr.hpp"
#include "db/sql_conn_RAII.hpp"
#include "utils/enums.hpp"
#include "utils/snowflake.hpp"

namespace model = tcs::model;
namespace json = boost::json;
namespace utils = tcs::utils;

using SqlConnRAII = tcs::db::SqlConnRAII;
using SnowFlake = tcs::utils::SnowFlake;
using UserClaims = tcs::model::UserClaims;

namespace tcs {
namespace core {
void WSHandler::handle_message(const std::string& msg_ptr, UserClaims user_claims) {
    SqlConnRAII conn;
    try {
        std::string user_id_str = std::to_string(user_claims.id);
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

        conn.begin_transaction();

        if (type == "private_message") {
            // todo: 好友检测
            model::ClientPrivateMsg private_msg =
                json::value_to<model::ClientPrivateMsg>(jv.at("data"));
            u64 msg_id = SnowFlake::next_id();

            int updated_row1 = conn.execute_update(
                "INSERT INTO messages (id, room_id, sender_id, content) VALUES (?, ?, ?, ?)",
                msg_id, private_msg.room_id, user_claims.id, private_msg.content);
            if (updated_row1 != 1) {
                throw std::runtime_error(
                    std::string("Failed to insert private message sent by \"") +
                    user_claims.username + "\"");
            }
            spdlog::debug("Inserted private messages {}", msg_id);

            int updated_row2 = conn.execute_update(
                "UPDATE rooms set last_message_id = ? WHERE id = ?", msg_id, private_msg.room_id);

            if (updated_row2 != 1) {
                throw std::runtime_error(
                    std::string("Failed to update last message in private room, sent by \"") +
                    user_claims.username + "\"");
            }
            spdlog::debug("Updated last message id {} in private room {}", msg_id,
                          private_msg.room_id);

            conn.commit();
            spdlog::debug("Transaction committed for private message {}", msg_id);

            model::ServerRespMsg<model::PrivateMsgToSend> private_msg_to_send = {
                .type = utils::ServerRespType::PMsgToSend,
                .data = model::PrivateMsgToSend{.sender_id = user_claims.id,
                                                .content = private_msg.content}};

            model::ServerRespMsg<std::string> msg_sent_info = {
                .type = utils::ServerRespType::MsgSentInfo, .data = std::string("")};

            WSSessionMgr::get().write_to(private_msg.other_user_id,
                                         json::serialize(json::value_from(private_msg_to_send)));

            // 私聊消息单独回一条送达信息
            WSSessionMgr::get().write_to(user_claims.id,
                                         json::serialize(json::value_from(msg_sent_info)));

        } else if (type == "group_message") {
            model::ClientGroupMsg group_msg = json::value_to<model::ClientGroupMsg>(jv.at("data"));

            u64 msg_id = SnowFlake::next_id();
            int updated_row1 = conn.execute_update(
                "INSERT INTO messages (id, room_id, sender_id, content) VALUES (?, ?, ?, ?)",
                msg_id, group_msg.room_id, user_claims.id, group_msg.content);

            if (updated_row1 != 1) {
                throw std::runtime_error(
                    std::string("Failed to insert group message into group\"") +
                    std::to_string(group_msg.room_id) + "\", sent by \"" + user_claims.username +
                    "\"");
            }
            spdlog::debug("Inserted group message {}", msg_id);

            int updated_row2 = conn.execute_update(
                "UPDATE rooms set last_message_id = ? WHERE id = ?", msg_id, group_msg.room_id);
            if (updated_row2 != 1) {
                throw std::runtime_error(
                    std::string("Failed to update last message in group room, sent by \"") +
                    user_claims.username + "\"");
            }
            spdlog::debug("Updated last message id {} in group room {}", msg_id, group_msg.room_id);
            conn.commit();
            spdlog::debug("Transaction committed for group message {}", msg_id);

            model::ServerRespMsg<model::GroupMsgToSend> group_msg_to_send = {
                .type = utils::ServerRespType::GMsgToSend,
                .data = model::GroupMsgToSend{.room_id = group_msg.room_id,
                                              .sender_id = user_claims.id,
                                              .content = group_msg.content}};

            // 群聊消息广播给所有群成员
            // 包括发送者，所以不需要单独回送送达信息
            WSSessionMgr::get().write_to_room(group_msg.room_id,
                                              json::serialize(json::value_from(group_msg_to_send)));
        }
    } catch (const std::exception& e) {
        conn.rollback();
        spdlog::error("Exception in handle websocket message:{}. Database rolled back", e.what());
    }
}
}  // namespace core
}  // namespace tcs
