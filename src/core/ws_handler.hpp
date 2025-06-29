#pragma once

#include <memory>
#include <string>

namespace tcs {
namespace core {
class WSHandler {
public:
    static void handle_message(const std::string& msg_ptr, const std::string& user_uuid);

private:
};
}  // namespace core
}  // namespace tcs