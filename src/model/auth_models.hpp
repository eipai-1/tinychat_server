#pragma once

#include <string>

#include "boost/json.hpp"

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

}  // namespace model
}  // namespace tcs