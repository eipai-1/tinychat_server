#pragma once

#include <string>

#include <boost/json.hpp>
#include <mysql/jdbc.h>

#include "utils/types.hpp"

namespace tcs::model {
struct Message {
    u64 id;
    u64 room_id;
    u64 sender_id;
    int content_type;
    std::string content;
    std::string created_at;

    static Message from_result_set(sql::ResultSet* rs) {
        Message msg;
        msg.id = rs->getUInt64("id");
        msg.room_id = rs->getUInt64("room_id");
        msg.sender_id = rs->getUInt64("sender_id");
        msg.content_type = rs->getInt("content_type");
        msg.content = rs->getString("content");
        msg.created_at = rs->getString("created_at");
        return msg;
    }
};

inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const Message& msg) {
    jv = boost::json::object{
        {"id", std::to_string(msg.id)},
        {"room_id", std::to_string(msg.room_id)},
        {"sender_id", std::to_string(msg.sender_id)},
        {"content_type", msg.content_type},
        {"content", msg.content},
        {"created_at", msg.created_at},
    };
}
}  // namespace tcs::model