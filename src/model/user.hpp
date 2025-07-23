#pragma once

#include <string>

#include <boost/json.hpp>

#include "utils/types.hpp"

namespace tcs {
namespace model {
struct User {
    u64 id;
    std::string username;
    std::string nickname;
    std::string email;
    std::string avatar_url;
    std::string created_at;
};

inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const User& user) {
    jv = boost::json::object{
        {"id", std::to_string(user.id)}, {"username", user.username},
        {"nickname", user.nickname},     {"email", user.email},
        {"avatar_url", user.avatar_url}, {"created_at", user.created_at},
    };
}

}  // namespace model
}  // namespace tcs