#pragma once
#include <string_view>
#include <map>

#include <boost/json.hpp>

#include "utils/types.hpp"

namespace tcs {
namespace utils {
enum class StatusCode : int {
    Success = 0,

    // 通用错误
    Forbidden = 403,
    BadRequest = 400,
    NotFound = 404,
    InternalServerError = 500,

    // 用户相关
    LoginFailed = 1001,
    RegFailed = 1002,
    UserNotFound = 1003,
    IncorrectPwd = 1004,

    // 聊天房间相关
    CreateRoomFailed = 2001,
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const StatusCode& code) {
    jv = static_cast<int>(code);
}

enum class RoomType : int {
    GROUP = 1,
    PRIVATE = 2,
};

enum class GroupRole : int {
    OWNER = 3,           // 群主
    ADMIN = 2,           // 管理员
    MEMBER = 1,          // 普通成员
    PRIVATE_MEMBER = 0,  // 私聊成员
};

enum class ServerRespType : int {
    // Message Sent Info
    // 用于通知消息已送达
    MsgSentInfo = 1,

    // Private Message To Send
    PMsgToSend = 2,

    // Group Message To Send
    GMsgToSend = 3,

    PermissionDenied = 4,
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const ServerRespType& type) {
    jv = static_cast<int>(type);
}

enum class WSType : int {
    PrivateMsg = 1,
    GroupMsg = 2,
    MsgSent = 3,
    Invalid = 0,
};
inline std::string_view ws_type_to_string(WSType type) {
    switch (type) {
        case WSType::PrivateMsg:
            return "private_message";
        case WSType::GroupMsg:
            return "group_message";
        case WSType::MsgSent:
            return "message_sent";
        default:
            return "invalid";
    }
}
inline WSType string_to_ws_type(std::string_view type) {
    static const std::map<std::string_view, WSType> type_map = {
        {"private_message", WSType::PrivateMsg},
        {"group_message", WSType::GroupMsg},
        {"message_sent", WSType::MsgSent},
    };
    auto it = type_map.find(type);
    if (it != type_map.end()) {
        return it->second;
    }
    return WSType::Invalid;
}

}  // namespace utils
}  // namespace tcs