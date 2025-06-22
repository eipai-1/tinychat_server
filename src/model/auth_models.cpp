#include "boost/json.hpp"

#include "model/auth_models.hpp"

namespace tcs {
namespace model {
LoginRequest tag_invoke(json::value_to_tag<LoginRequest>, const json::value& jv) {
    const json::object& obj = jv.as_object();

    return LoginRequest{.username = json::value_to<std::string>(obj.at("username")),
                        .password = json::value_to<std::string>(obj.at("password"))};
}

RegisterRequest tag_invoke(json::value_to_tag<RegisterRequest>, const json::value& jv) {
    const json::object& obj = jv.as_object();

    return RegisterRequest{.username = json::value_to<std::string>(obj.at("username")),
                           .password = json::value_to<std::string>(obj.at("password")),
                           .email = json::value_to<std::string>(obj.at("email")),
                           .nickname = json::value_to<std::string>(obj.at("nickname"))};
}

}  // namespace model
}  // namespace tcs