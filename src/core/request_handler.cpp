#include <string>
#include <chrono>
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

}  // namespace core
}  // namespace tcs