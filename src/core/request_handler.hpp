#pragma once

#include <boost/json.hpp>
#include <boost/beast/version.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "spdlog/spdlog.h"
#include "sodium.h"

#include "utils/net_utils.hpp"
#include "utils/status_code.hpp"
#include "db/sql_conn_RAII.hpp"
#include "model/auth_models.hpp"

namespace json = boost::json;
namespace model = tcs::model;

using StatusCode = tcs::utils::StatusCode;

template <typename Allocator>
using api_request = http::request<http::string_body, http::basic_fields<Allocator>>;

using SqlConnPool = tcs::db::SqlConnPool;
using SqlConnRAII = tcs::db::SqlConnRAII;

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
        if (req.target() == "/login") {
            return handle_login(std::move(req));
        } else if (req.target() == "/register") {
            return handle_register(std::move(req));
        } else {
            spdlog::warn("Unhandled request: {}", req.target());
            return bad_request(std::move(req), "Not Found");
        }
    }

private:
    static std::string generate_uuid() {
        static boost::uuids::random_generator uuid_gen_;
        return boost::uuids::to_string(uuid_gen_());
    }

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

    static std::string generate_login_token(int user_id, const std::string& username);

    template <typename Allocator>
    static http::message_generator bad_request(api_request<Allocator>&& req,
                                               const std::string& msg = std::string("")) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());

        json::object obj;
        obj["code"] = static_cast<int>(StatusCode::BadRequest);
        obj["message"] = "Bad Request: " + msg;
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

                if (!verify_password(login_request.password, hasd_pwd)) {
                    spdlog::debug("Login failed for username: {}, password incorrect.",
                                  login_request.username);
                    ApiResponse resp{StatusCode::IncorrectPwd, "Password incorrect", nullptr};
                    return create_json_response(http::status::unauthorized, req.version(),
                                                req.keep_alive(), json::value_from(resp));
                }
                int user_id = result_set->getInt("id");
                std::string token = generate_login_token(user_id, login_request.username);
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