#pragma once
#include <string>
#include <cstdint>
#include <vector>

#include <boost/json.hpp>

namespace tcs {
namespace model {
// Create group room request
struct CreateGRoomReq {
    std::string name;
};

inline CreateGRoomReq tag_invoke(boost::json::value_to_tag<CreateGRoomReq>,
                                 const boost::json::value& jv) {
    const boost::json::object obj = jv.as_object();
    return CreateGRoomReq{.name = json::value_to<std::string>(obj.at("name"))};
}

struct CreateGRoomResp {
    u64 room_id;
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const CreateGRoomResp& resp) {
    jv = boost::json::object{{"room_id", std::to_string(resp.room_id)}};
}

// Create private room Request
struct CreatePRoomReq {
    u64 other_id;
};
inline CreatePRoomReq tag_invoke(boost::json::value_to_tag<CreatePRoomReq>,
                                 const boost::json::value& jv) {
    const boost::json::object obj = jv.as_object();
    return CreatePRoomReq{.other_id = std::stoull(json::value_to<std::string>(obj.at("other_id")))};
}

struct CreatePRoomResp {
    u64 room_id;
};
inline void tag_invoke(boost::json::value_from_tag, boost::json::value& jv,
                       const CreatePRoomResp& resp) {
    jv = boost::json::object{{"room_id", std::to_string(resp.room_id)}};
}

// Group Room Invitation Request
struct GRoomInvtReq {
    u64 invitee_id;
};
inline GRoomInvtReq tag_invoke(boost::json::value_to_tag<GRoomInvtReq>,
                               const boost::json::value& jv) {
    const boost::json::object obj = jv.as_object();
    return GRoomInvtReq{
        .invitee_id = std::stoull(json::value_to<std::string>(obj.at("invitee_id"))),
    };
}

}  // namespace model
}  // namespace tcs