#include <string>
#include <chrono>
#include <filesystem>

#include <boost/json.hpp>
#include "jwt-cpp/jwt.h"
#include "jwt-cpp/traits/boost-json/traits.h"

#include "core/request_handler.hpp"
#include "db/sql_conn_RAII.hpp"
#include "utils/config.hpp"

using AppConfig = tcs::utils::AppConfig;

namespace tcs {
namespace core {
std::string RequestHandler::hash_password(const std::string& plain_password) {
    char hashed_password[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(hashed_password, plain_password.c_str(), plain_password.length(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw std::runtime_error("Failed to hash password with Argon2id");
    }

    return std::string(hashed_password);
}

bool RequestHandler::verify_password(const std::string& plain_password,
                                     const std::string& stored_hash) {
    if (crypto_pwhash_str_verify(stored_hash.c_str(), plain_password.c_str(),
                                 plain_password.length()) == 0) {
        return true;
    }
    return false;
}

UserClaims RequestHandler::extract_user_claims(const std::string& token) {
    using traits = jwt::traits::boost_json;
    jwt::decoded_jwt<traits> decoded_token = jwt::decode<traits>(token);
    if (!decoded_token.has_payload_claim("username")) {
        throw std::runtime_error("Token does not contain username claim");
    }
    u64 id = std::stoull(decoded_token.get_subject());
    std::string username = decoded_token.get_payload_claim("username").as_string();

    return UserClaims{
        .id = id,
        .username = username,
    };
}

std::string RequestHandler::generate_private_room_uuid(std::string u1, std::string u2) {
    if (u1 > u2) {
        std::swap(u1, u2);
    }
    std::string combined_key = u1 + "::" + u2;

    constexpr size_t HASH_BYTES = 20;
    unsigned char hash[HASH_BYTES];

    crypto_generichash(hash, HASH_BYTES,
                       reinterpret_cast<const unsigned char*>(combined_key.c_str()),
                       combined_key.length(), nullptr, 0);
    std::string hash_hex = bytes_to_hex(hash, HASH_BYTES);

    return "pr-" + hash_hex;
}

std::string_view RequestHandler::extract_target_param(std::string_view target, std::size_t index) {
    size_t start = 0;
    size_t end = 0;
    size_t current = 0;

    while (current <= index) {
        start = end;
        if (start >= target.size()) return {};  // 不足这么多段

        // 跳过开头的 '/'
        if (target[start] == '/') ++start;

        end = target.find('/', start);
        if (end == std::string_view::npos) {
            end = target.size();
        }

        if (current == index) {
            return target.substr(start, end - start);
        }

        ++current;
    }
    return std::string_view();
}

std::string RequestHandler::bytes_to_hex(const unsigned char* bytes, std::size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return ss.str();
}

std::string RequestHandler::generate_login_token(const std::string& username, u64 id) {
    const std::string jwt_secret = AppConfig::get().server().jwt_secret();

    using traits = jwt::traits::boost_json;

    auto token = jwt::create<traits>()
                     .set_type("JWS")
                     .set_issuer("tinychat_server")
                     .set_subject(std::to_string(id))
                     //.set_id(std::to_string(user_id))
                     .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24) * 30)
                     .set_payload_claim("username", jwt::basic_claim<traits>(username))
                     .sign(jwt::algorithm::hs256{jwt_secret});

    spdlog::debug("Generated JWT token:\"{}\" for \"{}\"", token, username);

    return token;
}

http::response<http::string_body> RequestHandler::error_resp(const ReqContext& ctx, StatusCode code,
                                                             const std::string& msg) {
    http::response<http::string_body> res;
    switch (code) {
        case StatusCode::BadRequest:
            res = http::response<http::string_body>{http::status::bad_request, ctx.version};
            break;

        case StatusCode::InternalServerError:
            res =
                http::response<http::string_body>{http::status::internal_server_error, ctx.version};
            break;

        case StatusCode::Forbidden:
            res = http::response<http::string_body>{http::status::forbidden, ctx.version};
            break;

        case StatusCode::NotFound:
            res = http::response<http::string_body>{http::status::not_found, ctx.version};
            break;

        default:
            res =
                http::response<http::string_body>{http::status::internal_server_error, ctx.version};
            spdlog::error("Unhandled status code: {}", static_cast<int>(code));
    }
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(ctx.keep_alive);

    json::object obj;
    obj["code"] = static_cast<int>(code);
    obj["message"] = "Bad Request" + msg;
    obj["data"] = nullptr;
    res.body() = json::serialize(obj);

    res.prepare_payload();
    return res;
}

std::string_view RequestHandler::mime_type(std::string_view path) {
    using boost::beast::iequals;
    auto const ext = [&path] {
        auto const pos = path.rfind(".");
        if (pos == std::string_view::npos) return std::string_view{};
        return path.substr(pos);
    }();

    if (iequals(ext, ".htm")) return "text/html";
    if (iequals(ext, ".html")) return "text/html";
    if (iequals(ext, ".php")) return "text/html";
    if (iequals(ext, ".css")) return "text/css";
    if (iequals(ext, ".txt")) return "text/plain";
    if (iequals(ext, ".js")) return "application/javascript";
    if (iequals(ext, ".json")) return "application/json";
    if (iequals(ext, ".xml")) return "application/xml";
    if (iequals(ext, ".swf")) return "application/x-shockwave-flash";
    if (iequals(ext, ".flv")) return "video/x-flv";
    if (iequals(ext, ".png")) return "image/png";
    if (iequals(ext, ".jpe")) return "image/jpeg";
    if (iequals(ext, ".jpeg")) return "image/jpeg";
    if (iequals(ext, ".jpg")) return "image/jpeg";
    if (iequals(ext, ".gif")) return "image/gif";
    if (iequals(ext, ".bmp")) return "image/bmp";
    if (iequals(ext, ".ico")) return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff")) return "image/tiff";
    if (iequals(ext, ".tif")) return "image/tiff";
    if (iequals(ext, ".svg")) return "image/svg+xml";
    if (iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/octet-stream";  // 默认的二进制流类型
}

http::message_generator RequestHandler::handle_assets(const ReqContext& ctx) {
    try {
        if (ctx.method != http::verb::get) {
            return error_resp(ctx, StatusCode::BadRequest, " Method Not Allowed");
        }
        std::string_view asset_path = ctx.target;          // 先创建一个副本或引用
        asset_path.remove_prefix(sizeof("/assets/") - 1);  // -1 是为了去掉字符串末尾的 '\0'

        if (asset_path.empty() || asset_path.find("..") != std::string_view::npos) {
            return error_resp(ctx, StatusCode::BadRequest, " Invalid asset path");
        }

        std::string utf8_path_str = AppConfig::get().server().doc_root() + std::string(asset_path);

        std::filesystem::path asset_file_path(asset_path);
        std::filesystem::path doc_root_path(AppConfig::get().server().doc_root());
        std::filesystem::path full_path = doc_root_path / asset_file_path;

        http::file_body::value_type body;
        beast::error_code ec;

        std::u8string path_for_beast = full_path.u8string();

        body.open(reinterpret_cast<const char*>(path_for_beast.data()), beast::file_mode::scan, ec);

        if (ec == beast::errc::no_such_file_or_directory) {
            spdlog::warn("Asset file not found: {}", utf8_path_str);
            return error_resp(ctx, StatusCode::NotFound, " Asset not found");
        }

        if (ec) {
            throw std::runtime_error("Failed to open asset file: " + ec.message());
        }

        auto const size = body.size();

        http::response<http::file_body> res{std::piecewise_construct,
                                            std::make_tuple(std::move(body)),
                                            std::make_tuple(http::status::ok, ctx.version)};
        res.set(http::field::server, "TinyChatServer");
        res.set(http::field::content_type, mime_type(utf8_path_str));
        res.set(http::field::content_length, std::to_string(size));
        res.keep_alive(ctx.keep_alive);

        return res;
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_assets: {}", e.what());
        return error_resp(ctx, StatusCode::InternalServerError, " Server Error");
    }
}

}  // namespace core
}  // namespace tcs