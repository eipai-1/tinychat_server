#include <chrono>
#include <boost/json.hpp>
#include "jwt-cpp/jwt.h"
#include "jwt-cpp/traits/boost-json/traits.h"

#include "core/request_handler.hpp"
#include "db/sql_conn_RAII.hpp"
#include "utils/config.hpp"

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
std::string RequestHandler::generate_login_token(int user_id, const std::string& username) {
    const std::string jwt_secret = AppConfig::getConfig()->server.jwt_secret;

    using traits = jwt::traits::boost_json;

    auto token = jwt::create<traits>()
                     .set_type("JWS")
                     .set_issuer("tinychat_server")
                     .set_subject(std::to_string(user_id))
                     //.set_id(std::to_string(user_id))
                     .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24) * 7)
                     .set_payload_claim("username", jwt::basic_claim<traits>(username))
                     .sign(jwt::algorithm::hs256{jwt_secret});

    spdlog::debug("Generated JWT token: {} for {}", token, username);

    return token;
}
}  // namespace core
}  // namespace tcs