#pragma once

#include <string>

#include <boost/json.hpp>

#include "utils/types.hpp"

namespace tcs {
namespace model {
struct Room {
    u64 id;
    i8 type;
    std::string name;
    std::string description;
    std::string avatar_url;
    u64 last_message_id;
    i32 member_count;
    std::string created_at;
};

inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const Room& room) {
    jv = boost::json::object{
        {"id", std::to_string(room.id)},
        {"type", room.type},
        {"name", room.name},
        {"description", room.description},
        {"avatar_url", room.avatar_url},
        {"last_message_id", std::to_string(room.last_message_id)},
        {"member_count", room.member_count},
        {"created_at", room.created_at},
    };
};

}  // namespace model
}  // namespace tcs