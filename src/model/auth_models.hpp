#pragma once

#include <string>

#include "boost/json.hpp"

#include "model/user.hpp"
#include "utils/types.hpp"

namespace json = boost::json;

namespace tcs {
namespace model {
struct LoginRequest {
    std::string username;
    std::string password;
};
LoginRequest tag_invoke(json::value_to_tag<LoginRequest>, const json::value& jv);

struct RegisterRequest {
    std::string username;
    std::string password;
    std::string email;
    std::string nickname;
};
RegisterRequest tag_invoke(json::value_to_tag<RegisterRequest>, const json::value& jv);

struct LoginResp {
    std::string token;
    User user;
};
inline void tag_invoke(json::value_from_tag, json::value& jv, const LoginResp& resp) {
    jv = json::object{
        {"token", resp.token},
        {"user", json::value_from(resp.user)},
    };
}

struct UserClaims {
    u64 id;
    std::string username;
};

}  // namespace model
}  // namespace tcs