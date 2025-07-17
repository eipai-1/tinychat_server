#pragma once

#include <string>
#include <cstdint>
#include <boost/json.hpp>

#include "utils/enums.hpp"
#include "utils/types.hpp"

namespace json = boost::json;

namespace tcs {
namespace model {
struct ClientPrivateMsg {
    u64 room_id;
    u64 other_user_id;
    std::string content;
};
// inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const
// ClientPrivateMsg& msg) {
//     jv = json::object{{"other_user_uuid", msg.other_user_uuid}, {"content", msg.content}};
// }
inline ClientPrivateMsg tag_invoke(json::value_to_tag<ClientPrivateMsg>, const json::value& jv) {
    const json::object& obj = jv.as_object();
    return ClientPrivateMsg{
        .room_id = std::stoull(json::value_to<std::string>(obj.at("room_id"))),
        .other_user_id = std::stoull(json::value_to<std::string>(obj.at("other_user_id"))),
        .content = json::value_to<std::string>(obj.at("content"))};
}

struct ClientGroupMsg {
    u64 room_id;
    std::string content;
};
inline ClientGroupMsg tag_invoke(json::value_to_tag<ClientGroupMsg>, const json::value& jv) {
    const json::object& obj = jv.as_object();
    return ClientGroupMsg{.room_id = json::value_to<u64>(obj.at("room_id")),
                          .content = json::value_to<std::string>(obj.at("content"))};
}

template <typename T>
struct ServerRespMsg {
    utils::ServerRespType type;
    T data;
};

template <typename T>
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const ServerRespMsg<T> resp) {
    jv = boost::json::object{{"type", json::value_from(resp.type)},
                             {"data", json::value_from(resp.data)}};
}

struct PrivateMsgToSend {
    u64 sender_id;
    std::string content;
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const PrivateMsgToSend& msg) {
    jv = boost::json::object{{"receiver_id", std::to_string(msg.sender_id)},
                             {"content", msg.content}};
}

struct GroupMsgToSend {
    u64 room_id;
    u64 sender_id;
    std::string content;
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const GroupMsgToSend& msg) {
    jv = boost::json::object{{"room_id", std::to_string(msg.room_id)},
                             {"sender_id", std::to_string(msg.sender_id)},
                             {"content", msg.content}};
}

}  // namespace model
}  // namespace tcs