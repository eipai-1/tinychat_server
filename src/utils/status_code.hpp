#pragma once
#include <boost/json.hpp>

namespace tcs {
namespace utils {
enum class StatusCode : int {
    Success = 0,

    // 通用错误
    BadRequest = 1000,

    // 用户相关错误
    LoginFailed = 1001,
    RegFailed = 1002,
    UserNotFound = 1003,
    IncorrectPwd = 1004,
};

inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const StatusCode& code) {
    jv = static_cast<int>(code);
}

}  // namespace utils
}  // namespace tcs