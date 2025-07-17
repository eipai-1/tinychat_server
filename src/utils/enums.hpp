#pragma once
#include <boost/json.hpp>

#include "utils/types.hpp"

namespace tcs {
namespace utils {
enum class StatusCode : int {
    Success = 0,

    // 通用错误
    Forbidden = 403,
    BadRequest = 400,
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
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const ServerRespType& type) {
    jv = static_cast<int>(type);
}

}  // namespace utils
}  // namespace tcs