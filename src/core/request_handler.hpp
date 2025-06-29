#pragma once

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
#include "utils/types.hpp"

namespace json = boost::json;
namespace model = tcs::model;

using StatusCode = tcs::utils::StatusCode;
using RoomType = tcs::utils::RoomType;

template <typename Allocator>
using api_request = http::request<http::string_body, http::basic_fields<Allocator>>;

using SqlConnPool = tcs::db::SqlConnPool;
using SqlConnRAII = tcs::db::SqlConnRAII;
using UserClaims = tcs::model::UserClaims;

namespace tcs {
namespace core {
template <typename T>
struct ApiResponse {
    StatusCode code;
    std::string message;
    T data;
};

class RequestHandler {
public:
    template <typename Allocator>
    static http::message_generator handle_request(std::string_view doc_root,
                                                  api_request<Allocator>&& req) {
        if (req.target() == "/api/login") {
            return handle_login(std::move(req));
        } else if (req.target() == "/api/register") {
            return handle_register(std::move(req));
        } else if (req.target() == "/api/group_room") {
            return create_g_room(std::move(req));
        } else if (req.target() == "/api/private_room") {
            return create_p_room(std::move(req));
        } else if (req.target().starts_with("/api/rooms/")) {
            // return handle_chat_room(std::move(req));
        } else {
            spdlog::warn("Unhandled request: {}", req.target());
            return bad_request(std::move(req), "Not Found");
        }
    }

    static UserClaims extract_user_claims(const std::string& token);

    static std::string generate_private_room_uuid(std::string u1, std::string u2);
    static std::string generate_uuid() {
        static boost::uuids::random_generator uuid_gen_;
        return boost::uuids::to_string(uuid_gen_());
    }

private:
    static std::string bytes_to_hex(const unsigned char* bytes, std::size_t len);

    static std::string hash_password(const std::string& plain_password);
    static bool verify_password(const std::string& plain_password, const std::string& stored_hash);

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

    // 默认建群者是群主
    template <typename Allocator>
    static http::message_generator create_g_room(api_request<Allocator>&& req) {
        if (req.method() != http::verb::post) {
            return bad_request(std::move(req));
        }

        try {
            json::value jv = json::parse(req.body());
            model::CreateGRoomReq create_g_room_req = json::value_to<model::CreateGRoomReq>(jv);
            SqlConnRAII conn;
            std::string room_uuid = generate_uuid();

            UserClaims user_claims =
                extract_user_claims(std::string(req[http::field::authorization]));

            int updated_row = conn.execute_update(
                "INSERT INTO rooms (uuid, name, type, owner_uuid) VALUES (?, ?, ?, ?)", room_uuid,
                create_g_room_req.name, static_cast<i8>(RoomType::GROUP), user_claims.uuid);
            if (updated_row == 1) {
                ApiResponse<model::CreateGRoomResp> resp{
                    StatusCode::Success, "Group room created successfully",
                    model::CreateGRoomResp{.room_uuid = room_uuid}};

                spdlog::info("Group room created: {}, UUID: {}", create_g_room_req.name, room_uuid);

                return create_json_response(http::status::ok, req.version(), req.keep_alive(),
                                            json::value_from(resp));
            } else {
                spdlog::error("Failed to create group room: {}", create_g_room_req.name);
                ApiResponse<std::nullptr_t> resp{StatusCode::CreateRoomFailed,
                                                 "Group room creation failed", nullptr};
                return create_json_response(http::status::bad_request, req.version(),
                                            req.keep_alive(), json::value_from(resp));
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception during group room creation: {}", e.what());
            return bad_request(std::move(req), " Server Error");
        }
    }

    template <typename Allocator>
    static http::message_generator create_p_room(api_request<Allocator>&& req) {
        if (req.method() != http::verb::post) {
            return bad_request(std::move(req));
        }
        try {
            json::value jv = json::parse(req.body());
            model::CreatePRoomReq create_g_room_req = json::value_to<model::CreatePRoomReq>(jv);

            SqlConnRAII conn;

            std::string room_uuid = generate_uuid();
            UserClaims user_claims =
                extract_user_claims(std::string(req[http::field::authorization]));

            int updated_row = conn.execute_update(
                "INSERT INTO rooms (uuid, type, owner_uuid) VALUES (?, ?, ?, ?)", room_uuid,
                static_cast<i8>(RoomType::PRIVATE), user_claims.uuid);

            if (updated_row != 1) {
                spdlog::error("Failed to create private room for user: {}", user_claims.username);
                throw std::runtime_error("Failed to create private room");
            }

            updated_row = conn.execute_update(
                "INSERT INTO room_members (room_uuid, user_uuid) VALUES (?, ?)", room_uuid);

        } catch (const std::exception& e) {
            spdlog::error("Exception during private room creation: {}", e.what());
            return bad_request(std::move(req), " Server Error");
        }
    }

    static std::string generate_login_token(const std::string& username, const std::string& uuid);

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
                "SELECT * FROM users WHERE username = ?", login_request.username));

            if (result_set->next()) {
                std::string hasd_pwd = result_set->getString("password_hash");
                std::string uuid = result_set->getString("uuid");

                if (!verify_password(login_request.password, hasd_pwd)) {
                    spdlog::debug("Login failed for username: {}, password incorrect.",
                                  login_request.username);
                    ApiResponse resp{StatusCode::IncorrectPwd, "Password incorrect", nullptr};
                    return create_json_response(http::status::unauthorized, req.version(),
                                                req.keep_alive(), json::value_from(resp));
                }
                std::string token = generate_login_token(login_request.username, uuid);
                ApiResponse resp{StatusCode::Success, "Login successful", token};

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
                std::string uuid = generate_uuid();
                int updated_row = conn.execute_update(
                    "INSERT INTO users (uuid, username, password_hash, email, nickname) "
                    "VALUES (?, ?, ?, ?, ?)",
                    uuid, register_request.username, hashed_pwd, register_request.email,
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
};

template <typename T>
void tag_invoke(json::value_from_tag, json::value& jv, const ApiResponse<T>& resp) {
    jv = {{"code", json::value_from(resp.code)},
          {"message", resp.message},
          {"data", json::value_from(resp.data)}};
}

}  // namespace core
}  // namespace tcs