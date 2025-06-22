cmake_minimum_required(VERSION 3.10.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(tinychat_server VERSION 0.1.0 LANGUAGES C CXX)

set(CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)


if(UNIX AND NOT APPLE)
find_package(libmysqlclient REQUIRED)
endif()

list(APPEND CMAKE_PREFIX_PATH
    "D:/program/cpp/libraries/mysql-connector-c++-9.3.0-winx64-Debug"
)

find_package(libsodium REQUIRED)
find_package(jwt-cpp)
find_package(mysql-concpp REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED COMPONETS system json)

set(SEMAPHORE_MAX_VALUE 4096 CACHE STRING "Maximum capacity for the task queue semaphore")

set(HEADERS
    src/tinychat_server.hpp
    src/pch.hpp
    src/core/listener.hpp
    src/core/session.hpp
    src/core/request_handler.hpp
    src/db/sql_conn_pool.hpp
    src/db/sql_conn_RAII.hpp
    src/pool/thread_pool.hpp
    src/utils/net_utils.hpp
    src/utils/config.hpp
    src/utils/status_code.hpp
    src/model/auth_models.hpp
)

set(SOURCES
    src/db/sql_conn_pool.cpp
    src/db/sql_conn_RAII.cpp
    src/tinychat_server.cpp
    src/core/listener.cpp
    src/core/session.cpp
    src/core/request_handler.cpp
    src/pool/thread_pool.cpp
    src/utils/config.cpp
    src/model/auth_models.cpp
)

add_executable(tinychat_server 
    ${SOURCES}
    ${HEADERS}
    src/main.cpp
    #src/listener.cpp
    #src/chat_session.cpp
)

# --- test client ---

add_executable(test_client
    ${SOURCES}
    ${HEADERS}
    src/test_client.cpp
)

target_include_directories(test_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(test_client PRIVATE
    Boost::system
    Boost::json
    spdlog::spdlog
    mysql::concpp-jdbc-static
    jwt-cpp::jwt-cpp
    libsodium::libsodium
)

# -------------------

target_precompile_headers(tinychat_server PRIVATE src/pch.hpp)

target_include_directories(tinychat_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tinychat_server PRIVATE
    Boost::system
    Boost::json
    spdlog::spdlog
    mysql::concpp-jdbc-static
    jwt-cpp::jwt-cpp
    libsodium::libsodium
)

if(WIN32)
    # 如果是 Windows 平台
    message(STATUS "----- Configuring for Windows platform.")
    target_compile_definitions(tinychat_server PRIVATE PLATFORM_WINDOWS)
    target_compile_definitions(test_client PRIVATE PLATFORM_WINDOWS)

    
    # 你还可以添加其他 Windows 特有的宏
    # 例如，禁用某些烦人的警告
    #target_compile_definitions(tinychat_server PRIVATE _CRT_SECURE_NO_WARNINGS)

elseif(UNIX AND NOT APPLE)
    # 如果是 Linux 平台 (UNIX 为真，但 APPLE 为假)
    message(STATUS "Configuring for Linux platform.")
    target_compile_definitions(tinychat_server PRIVATE PLATFORM_LINUX)
    
elseif(APPLE)
    # 如果是 macOS 平台
    message(STATUS "Configuring for macOS platform.")
    target_compile_definitions(tinychat_server PRIVATE PLATFORM_MACOS)

else()
    # 其他未知平台
    message(WARNING "Unsupported platform detected.")
endif()

target_compile_definitions(tinychat_server PRIVATE
    "SEMAPHORE_MAX_VALUE=${SEMAPHORE_MAX_VALUE}"
)

target_compile_definitions(test_client PRIVATE
    "SEMAPHORE_MAX_VALUE=${SEMAPHORE_MAX_VALUE}"
)

#target_compile_definitions(test_client PRIVATE 
#    STATIC_CONCPP
#    # 把CMake变量变成C++宏
#    "-DMYSQL_PLUGIN_DIR=\"${mysql-concpp_PLUGIN_DIR}\""
#)