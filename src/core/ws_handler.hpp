#pragma once

#include <memory>
#include <string>

#include "model/auth_models.hpp"
#include "utils/types.hpp"

namespace tcs {
namespace core {
class WSHandler {
public:
    static void handle_message(const std::string& msg_ptr, tcs::model::UserClaims user_claims);

private:
};
}  // namespace core
}  // namespace tcs