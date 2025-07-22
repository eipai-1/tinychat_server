#pragma once

#include <string>
#include <optional>
#include <vector>

#include <cstddef>
#include <boost/json.hpp>
#include <boost/beast/version.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "spdlog/spdlog.h"
#include "sodium.h"

#include "utils/net_utils.hpp"
#include "utils/enums.hpp"
#include "db/sql_conn_RAII.hpp"
#include "model/auth_models.hpp"
#include "model/chat_models.hpp"
#include "model/user.hpp"
#include "model/room.hpp"
#include "utils/types.hpp"
#include "utils/snowflake.hpp"

namespace json = boost::json;
namespace model = tcs::model;

using StatusCode = tcs::utils::StatusCode;
using RoomType = tcs::utils::RoomType;
using SnowFlake = tcs::utils::SnowFlake;

template <typename Allocator>
using api_request = http::request<http::string_body, http::basic_fields<Allocator>>;

using SqlConnPool = tcs::db::SqlConnPool;
using SqlConnRAII = tcs::db::SqlConnRAII;

using User = tcs::model::User;
using UserClaims = tcs::model::UserClaims;
using LoginResp = tcs::model::LoginResp;
using Room = model::Room;

namespace tcs {
namespace core {
template <typename T>
struct ApiResponse {
    StatusCode code;
    std::string message;
    T data;
};

// 准备重构

// Request context
struct ReqContext {
    unsigned version;
    bool keep_alive;
    http::verb method;
    std::string target;
    std::optional<json::value> jv_opt;
    std::optional<UserClaims> user_claims_opt;
};

class RequestHandler {
public:
    template <typename Allocator>
    static http::message_generator handle_request(std::string_view doc_root,
                                                  api_request<Allocator>&& req) {
        ReqContext ctx{.version = req.version(),
                       .keep_alive = req.keep_alive(),
                       .method = req.method(),
                       .target = std::string(req.target()),
                       .jv_opt = std::nullopt,
                       .user_claims_opt = std::nullopt};

        if (req.find(http::field::authorization) != req.end()) {
            ctx.user_claims_opt = extract_user_claims(req[http::field::authorization]);
        }

        if (!req.body().empty()) {
            ctx.jv_opt = json::parse(req.body());
        }

        if (req.target() == "/api/login") {
            return handle_login(std::move(req));
        } else if (req.target() == "/api/register") {
            return handle_register(std::move(req));
        } else if (req.target() == "/api/group_room") {
            return create_g_room(std::move(req));
        } else if (req.target() == "/api/private_room") {
            return create_p_room(std::move(req));
        } else if (req.target().starts_with("/api/rooms/")) {
            return handle_chat_room(std::move(req));
        } else if (req.target() == "/users/me/rooms") {
            return query_rooms(ctx);
        } else if (req.target().starts_with("/assets")) {
            return handle_assets(ctx);
        } else {
            spdlog::warn("Unhandled request: {}", req.target());
            return bad_request(std::move(req), " Not Found");
        }
    }

    static UserClaims extract_user_claims(const std::string& token);

    static std::string generate_private_room_uuid(std::string u1, std::string u2);
    static std::string generate_uuid() {
        static boost::uuids::random_generator uuid_gen_;
        return boost::uuids::to_string(uuid_gen_());
    }

private:
    // 提取请求路径参数
    // 例：/api/rooms/some_room_uuid/members
    // -----0----1----------2----------3---
    static std::string_view extract_target_param(std::string_view target, std::size_t index);

    static std::string bytes_to_hex(const unsigned char* bytes, std::size_t len);

    static std::string hash_password(const std::string& plain_password);
    static bool verify_password(const std::string& plain_password, const std::string& stored_hash);

    static std::string generate_login_token(const std::string& username, u64 id);

    static std::string_view mime_type(std::string_view path);

    static http::response<http::string_body> create_json_response(http::status status,
                                                                  unsigned version, bool keep_alive,
                                                                  const json::value& jv) {
        http::response<http::string_body> res{status, version};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(keep_alive);
        res.body() = json::serialize(jv);
        res.prepare_payload();
        return res;
    }

    template <typename T>
    static http::response<http::string_body> create_json_response(http::status status,
                                                                  unsigned version, bool keep_alive,
                                                                  ApiResponse<T> api_resp) {
        http::response<http::string_body> res{status, version};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(keep_alive);
        res.body() = json::serialize(json::value_from(api_resp));
        res.prepare_payload();
        return res;
    }

    // 默认建群者是群主
    template <typename Allocator>
    static http::message_generator create_g_room(api_request<Allocator>&& req) {
        if (req.method() != http::verb::post) {
            return bad_request(std::move(req));
        }

        SqlConnRAII conn;
        try {
            json::value jv = json::parse(req.body());
            model::CreateGRoomReq create_g_room_req = json::value_to<model::CreateGRoomReq>(jv);

            //-----事务开始-----
            conn.begin_transaction();

            u64 room_id = SnowFlake::next_id();

            UserClaims user_claims =
                extract_user_claims(std::string(req[http::field::authorization]));

            // 1.创建房间
            int updated_row1 = conn.execute_update(
                "INSERT INTO rooms (id, name, type, owner_id) VALUES (?, ?, ?, ?)", room_id,
                create_g_room_req.name, static_cast<int>(RoomType::GROUP), user_claims.id);

            if (updated_row1 != 1) {
                throw std::runtime_error("Failed to create group room");
            }

            spdlog::info("Created group room: {}, owner: {}", room_id, user_claims.username);

            // 2.添加创建者为群主
            int updated_row2 = conn.execute_update(
                "INSERT INTO room_members (room_id, user_id, role) VALUES (?, ?, ?)", room_id,
                user_claims.id, static_cast<int>(utils::GroupRole::OWNER));

            if (updated_row2 != 1) {
                throw std::runtime_error("Failed to add owner to group room");
            }

            spdlog::info("Added owner {} to group room {}", user_claims.username, room_id);

            return create_json_response(
                http::status::ok, req.version(), req.keep_alive(),
                json::value_from(ApiResponse<model::CreateGRoomResp>{
                    StatusCode::Success, "Room created success, creator will be the owner.",
                    model::CreateGRoomResp{.room_id = room_id}}));

        } catch (const std::exception& e) {
            spdlog::error("Exception during group room creation: {}", e.what());

            conn.rollback();

            return bad_request(std::move(req), " Server Error");
        }
    }

    // 默认建群者是群主
    template <typename Allocator>
    static http::message_generator create_p_room(api_request<Allocator>&& req) {
        if (req.method() != http::verb::post) {
            return bad_request(std::move(req));
        }

        SqlConnRAII conn;
        try {
            json::value jv = json::parse(req.body());
            model::CreatePRoomReq create_p_room_req = json::value_to<model::CreatePRoomReq>(jv);

            //-----事务开始-----
            conn.begin_transaction();

            u64 room_id = SnowFlake::next_id();

            UserClaims user_claims =
                extract_user_claims(std::string(req[http::field::authorization]));

            // 1.创建房间
            int updated_row1 = conn.execute_update("INSERT INTO rooms (id, type) VALUES (?, ?)",
                                                   room_id, static_cast<int>(RoomType::PRIVATE));

            if (updated_row1 != 1) {
                throw std::runtime_error("Failed to create group room");
            }

            spdlog::info("Created private room: {}", room_id);

            // 2.添加创建者为群主
            int updated_row2 = conn.execute_update(
                "INSERT INTO room_members (room_id, user_id, role)"
                "VALUES (?, ?, ?), (?, ?, ?)",
                room_id, user_claims.id, static_cast<int>(utils::GroupRole::PRIVATE_MEMBER),
                room_id, create_p_room_req.other_id,
                static_cast<int>(utils::GroupRole::PRIVATE_MEMBER));

            if (updated_row2 != 2) {
                throw std::runtime_error("Failed to add owner to group room");
            }

            spdlog::info("Added user\"{}\" and \"{}\" to private room \"{}\"", user_claims.id,
                         create_p_room_req.other_id, room_id);

            conn.commit();
            spdlog::info("Transaction committed for private room {}", room_id);

            return create_json_response(http::status::ok, req.version(), req.keep_alive(),
                                        json::value_from(ApiResponse<model::CreatePRoomResp>{
                                            StatusCode::Success, "Private room created success.",
                                            model::CreatePRoomResp{.room_id = room_id}}));

        } catch (const std::exception& e) {
            conn.rollback();

            spdlog::error("Exception during group room creation: {}. Database rolled back",
                          e.what());

            return bad_request(std::move(req), " Server Error");
        }
    }

    template <typename Allocator>
    static http::message_generator handle_chat_room(api_request<Allocator>&& req) {
        try {
            std::string_view target = req.target();
            u64 room_id = std::stoull(std::string(extract_target_param(target, 2)));
            std::string room_verb = std::string(extract_target_param(target, 3));

            if (room_verb.empty()) {
                if (req.method() == http::verb::delete_) {
                    return delete_room(std::move(req), room_id);
                }
            } else if (room_verb == "member") {
                return invite_member(std::move(req), room_id);
            } else {
                return bad_request(std::move(req), " target not found");
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception in handle_chat_room: {}", e.what());
            return bad_request(std::move(req), " Server Error");
        }
    }

    // 已确认方法为delete
    template <typename Allocator>
    static http::message_generator delete_room(api_request<Allocator>&& req, u64 room_id) {
        SqlConnRAII conn;
        conn.begin_transaction();

        try {
            // 权限检查
            UserClaims user_claims =
                extract_user_claims(std::string(req[http::field::authorization]));

            std::unique_ptr<sql::ResultSet> role_set(conn.execute_query(
                "SELECT role FROM room_members WHERE room_id = ? AND user_id = ?", room_id,
                user_claims.id));
            if (role_set->next()) {
                int role = role_set->getInt("role");
                if (role != static_cast<int>(utils::GroupRole::OWNER)) {
                    return error_resp(std::move(req), StatusCode::Forbidden, " Permission denied");
                }
            } else {
                return error_resp(std::move(req), StatusCode::BadRequest,
                                  " You are not a member in the group");
            }
            // 权限检查通过

            // 不检查群是否存在，正常情况应该存在

            // 首先删除群成员
            int deleted_member =
                conn.execute_update("DELETE FROM room_members WHERE room_id = ?", room_id);

            // 存在没有成员的群属于服务器错误
            if (deleted_member == 0) {
                throw std::runtime_error(std::string("There is a room \"") +
                                         std::to_string(room_id) + "\" which has no member");
            }
            spdlog::info("Deleted {} members from room {}", deleted_member, room_id);

            int deleted_room = conn.execute_update("DELETE FROM rooms WHERE id = ?", room_id);

            // 删除了群成员，但却没有删除群，属于服务器错误
            if (deleted_room == 0) {
                throw std::runtime_error("Failed to delete room");
            }

            conn.commit();

            spdlog::info("Room {} deleted successfully", room_id);

            return create_json_response(
                http::status::ok, req.version(), req.keep_alive(),
                ApiResponse<std::nullptr_t>{StatusCode::Success, "Room deleted successfully",
                                            nullptr});

        } catch (const std::exception& e) {
            conn.rollback();
            spdlog::error("Exception during room deletion: {}. Database rolled back", e.what());
            return error_resp(std::move(req), StatusCode::BadRequest, " Server error");
        }
    }

    template <typename Allocator>
    static http::message_generator invite_member(api_request<Allocator>&& req, u64 room_id) {
        if (req.method() != http::verb::post) {
            return bad_request(std::move(req), " Method Not Allowed");
        }
        // todo: 邀请处理
        model::GRoomInvtReq invt_req = json::value_to<model::GRoomInvtReq>(json::parse(req.body()));

        SqlConnRAII conn;
        UserClaims user_claims = extract_user_claims(std::string(req[http::field::authorization]));

        std::unique_ptr<sql::ResultSet> result_set(
            conn.execute_query("SELECT type FROM rooms WHERE id = ?", room_id));

        if (!result_set->next()) {
            spdlog::error("user {} trying to invited {} to a not found room {}", user_claims.id,
                          invt_req.invitee_id, room_id);
            return error_resp(std::move(req), StatusCode::BadRequest, " Room not found");
        }

        if (result_set->getInt("type") != static_cast<int>(RoomType::GROUP)) {
            spdlog::error("user {} trying to invited {} to a private room {}", user_claims.id,
                          invt_req.invitee_id, room_id);
            return error_resp(std::move(req), StatusCode::BadRequest, " Illegal request");
        }

        // 此处数据库约束会帮我们确保不会重复添加同一个成员
        int update_row = conn.execute_update(
            "INSERT INTO room_members (room_id, user_id, role) VALUE (?, ?, ?)", room_id,
            invt_req.invitee_id, static_cast<int>(utils::GroupRole::MEMBER));

        if (update_row != 1) {
            spdlog::error("Failed to invite user {} to room {}", invt_req.invitee_id, room_id);
            return bad_request(std::move(req), " Invite failed");
        }

        spdlog::info("User {} invited {} to group room {} and added to group success",
                     user_claims.id, invt_req.invitee_id, room_id);

        return create_json_response(
            http::status::ok, req.version(), req.keep_alive(),
            json::value_from(ApiResponse<std::nullptr_t>{
                StatusCode::Success, "Invitee successfully added to the group", nullptr}));
    }

    template <typename Allocator>
    static http::message_generator bad_request(api_request<Allocator>&& req,
                                               const std::string& msg = std::string("")) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());

        json::object obj;
        obj["code"] = static_cast<int>(StatusCode::BadRequest);
        obj["message"] = "Bad Request" + msg;
        obj["data"] = nullptr;
        res.body() = json::serialize(obj);

        res.prepare_payload();
        return res;
    }

    template <typename Allocator>
    static http::response<http::string_body> error_resp(api_request<Allocator>&& req,
                                                        StatusCode code,
                                                        const std::string& msg = std::string("")) {
        http::response<http::string_body> res;
        switch (code) {
            case StatusCode::BadRequest:
                res = http::response<http::string_body>{http::status::bad_request, req.version()};
                break;

            case StatusCode::InternalServerError:
                res = http::response<http::string_body>{http::status::internal_server_error,
                                                        req.version()};
                break;

            case StatusCode::Forbidden:
                res = http::response<http::string_body>{http::status::forbidden, req.version()};
                break;

            default:
                res = http::response<http::string_body>{http::status::internal_server_error,
                                                        req.version()};
                spdlog::error("Unhandled status code: {}", static_cast<int>(code));
        }
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());

        json::object obj;
        obj["code"] = static_cast<int>(code);
        obj["message"] = "Bad Request" + msg;
        obj["data"] = nullptr;
        res.body() = json::serialize(obj);

        res.prepare_payload();
        return res;
    }

    static http::response<http::string_body> error_resp(const ReqContext& ctx, StatusCode code,
                                                        const std::string& msg = std::string(""));

    template <typename Allocator>
    static http::message_generator handle_login(api_request<Allocator>&& req) {
        if (req.method() == http::verb::post) {
            // Handle login logic here
            json::value parsed_json;
            beast::error_code ec;

            parsed_json = json::parse(req.body(), ec);
            if (ec) {
                spdlog::warn("JSON parsed fail for /login: {}", ec.message());
                return bad_request(std::move(req), "Invalid JSON format");
            }

            model::LoginRequest login_request = json::value_to<model::LoginRequest>(parsed_json);

            SqlConnRAII conn;

            std::unique_ptr<sql::ResultSet> result_set(conn.execute_query(
                "SELECT id, password_hash, nickname, email, avatar_url, created_at FROM "
                "users WHERE username = ?",
                login_request.username));

            if (result_set->next()) {
                std::string hasd_pwd = result_set->getString("password_hash");

                if (!verify_password(login_request.password, hasd_pwd)) {
                    spdlog::debug("Login failed for username: {}, password incorrect.",
                                  login_request.username);
                    ApiResponse resp{StatusCode::IncorrectPwd, "Password incorrect", nullptr};
                    return create_json_response(http::status::unauthorized, req.version(),
                                                req.keep_alive(), json::value_from(resp));
                }

                User user{
                    .id = result_set->getUInt64("id"),
                    .username = login_request.username,
                    .nickname = result_set->getString("nickname"),
                    .email = result_set->getString("email"),
                    .avatar_url = result_set->getString("avatar_url"),
                    .created_at = result_set->getString("created_at"),
                };

                std::string token = generate_login_token(login_request.username, user.id);

                LoginResp login_resp{.token = token, .user = user};

                ApiResponse resp{StatusCode::Success, "Login successful", login_resp};

                return create_json_response(http::status::ok, req.version(), req.keep_alive(),
                                            json::value_from(resp));
            } else {
                spdlog::debug("Login failed for username: {}, user not found.",
                              login_request.username);
                ApiResponse resp{StatusCode::UserNotFound, "User not found", nullptr};

                return create_json_response(http::status::not_found, req.version(),
                                            req.keep_alive(), json::value_from(resp));
            }

        } else {
            return bad_request(std::move(req), "Bad Request");
        }
    }

    static http::message_generator query_rooms(const ReqContext& ctx) {
        try {
            if (ctx.method != http::verb::get) {
                return error_resp(ctx, StatusCode::BadRequest, " Method Not Allowed");
            }

            std::vector<Room> rooms;
            SqlConnRAII conn;

            // std::unique_ptr<sql::ResultSet> result_set(
            //     conn.execute_query("SELECT id, name, type, description, avatar_url, "
            //                        "last_message_id, memeber_cout, created_at FROM users WHERE"
            //                        ));
            std::unique_ptr<sql::ResultSet> room_ids(conn.execute_query(
                "SELECT room_id FROM room_members WHERE user_id = ?", ctx.user_claims_opt->id));

        } catch (const std::exception& e) {
            spdlog::error("Exception in query_rooms: {}", e.what());
            return error_resp(ctx, StatusCode::InternalServerError, " Server Error");
        }
    }

    template <typename Allocator>
    static http::message_generator handle_register(api_request<Allocator>&& req) {
        if (req.method() == http::verb::post) {
            try {
                beast::error_code ec;
                json::value jv = json::parse(req.body(), ec);
                if (ec) {
                    spdlog::warn("JSON parsed fail for /register: {}", ec.message());
                    return bad_request(std::move(req), "Invalid JSON format");
                }
                model::RegisterRequest register_request =
                    json::value_to<model::RegisterRequest>(jv);
                SqlConnRAII conn;
                std::string hashed_pwd = hash_password(register_request.password);
                u64 id = SnowFlake::next_id();
                int updated_row = conn.execute_update(
                    "INSERT INTO users (id, username, password_hash, email, nickname) "
                    "VALUES (?, ?, ?, ?, ?)",
                    id, register_request.username, hashed_pwd, register_request.email,
                    register_request.nickname);

                if (updated_row == 1) {
                    ApiResponse resp{StatusCode::Success, "Registration successful", nullptr};
                    json::value resp_json = json::value_from(resp);

                    spdlog::debug("User registered successfully: {}", register_request.username);

                    return create_json_response(http::status::ok, req.version(), req.keep_alive(),
                                                resp_json);
                } else if (updated_row == 0) {
                    spdlog::debug("Registration failed for username: {}",
                                  register_request.username);

                    ApiResponse resp{StatusCode::RegFailed, "Register Failed", nullptr};

                    return create_json_response(http::status::bad_request, req.version(),
                                                req.keep_alive(), json::value_from(resp));
                } else {
                    spdlog::error("Unexpected error during registration for username: {}",
                                  register_request.username);
                    return bad_request(std::move(req), "Server Error");
                }

            } catch (const std::exception& e) {
                spdlog::error("Exception during registration: {}", e.what());
                return bad_request(std::move(req), "Server Error");
            }

        } else {
            return bad_request(std::move(req));
        }
    }

    static http::message_generator handle_assets(const ReqContext& ctx);
};

template <typename T>
void tag_invoke(json::value_from_tag, json::value& jv, const ApiResponse<T>& resp) {
    jv = {{"code", json::value_from(resp.code)},
          {"message", resp.message},
          {"data", json::value_from(resp.data)}};
}

}  // namespace core
}  // namespace tcs