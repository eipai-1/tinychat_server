#pragma once

#include <string>
#include <cstdint>
#include <boost/json.hpp>

#include "utils/enums.hpp"

namespace json = boost::json;

namespace tcs {
namespace model {
struct PrivateMsg {
    std::string other_user_uuid;
    std::string content;
};
// inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const PrivateMsg&
// msg) {
//     jv = json::object{{"other_user_uuid", msg.other_user_uuid}, {"content", msg.content}};
// }
inline PrivateMsg tag_invoke(json::value_to_tag<PrivateMsg>, const json::value& jv) {
    const json::object& obj = jv.as_object();
    return PrivateMsg{.other_user_uuid = json::value_to<std::string>(obj.at("other_user_uuid")),
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
    std::string sender_uuid;
    std::string content;
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const PrivateMsgToSend& msg) {
    jv = boost::json::object{{"receiver_uuid", msg.sender_uuid}, {"content", msg.content}};
}

}  // namespace model
}  // namespace tcs