#pragma once
#include <string>
#include <cstdint>
#include <boost/json.hpp>

namespace json = boost::json;

namespace tcs {
namespace model {
// Create group room request
struct CreateGRoomReq {
    std::string name;
};

inline CreateGRoomReq tag_invoke(boost::json::value_to_tag<CreateGRoomReq>,
                                 const boost::json::value& jv) {
    const json::object obj = jv.as_object();
    return CreateGRoomReq{.name = json::value_to<std::string>(obj.at("name"))};
}

struct CreateGRoomResp {
    std::string room_uuid;
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const CreateGRoomResp& resp) {
    jv = json::object{{"room_uuid", resp.room_uuid}};
}

// Create private room Request
struct CreatePRoomReq {
    std::string other_uuid;
};
inline CreatePRoomReq tag_invoke(boost::json::value_to_tag<CreatePRoomReq>,
                                 const boost::json::value& jv) {
    const json::object obj = jv.as_object();
    return CreatePRoomReq{.other_uuid = json::value_to<std::string>(obj.at("other_uuid"))};
}

struct CreatePRoomResp {
    std::string room_uuid;
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const CreatePRoomResp& resp) {
    jv = json::object{{"room_uuid", resp.room_uuid}};
}

}  // namespace model
}  // namespace tcs