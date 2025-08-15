#pragma once

#include <string>

#include <boost/json.hpp>
#include <mysql/jdbc.h>

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

    static Room from_result_set(sql::ResultSet* rs) {
        Room room;
        room.id = rs->getUInt64("id");
        room.type = static_cast<i8>(rs->getInt("type"));
        room.name = rs->getString("name");
        room.description = rs->getString("description");
        room.avatar_url = rs->getString("avatar_url");
        room.last_message_id = rs->getUInt64("last_message_id");
        room.member_count = rs->getInt("member_count");
        room.created_at = rs->getString("created_at");
        return room;
    };
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
}

}  // namespace model
}  // namespace tcs