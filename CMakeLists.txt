cmake_minimum_required(VERSION 3.10.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(tinychat_server VERSION 0.1.0 LANGUAGES C CXX)

set(CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)

find_package(libmysqlclient REQUIRED)
find_package(Boost REQUIRED COMPONETS system json)

set(SEMAPHORE_MAX_VALUE 4096 CACHE STRING "Maximum capacity for the task queue semaphore")

set(HEADERS
    src/net_utils.hpp
    src/pch.hpp
    src/pool/thread_pool.hpp
    src/pool/sql_conn_pool.hpp
    src/pool/sql_conn_RAII.hpp
    src/utils/config.h
)

set(SOURCES
    src/pool/thread_pool.cpp
    src/pool/sql_conn_pool.cpp
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
    #nlohmann_json::nlohmann_json
    Boost::system
    Boost::json
    libmysqlclient::libmysqlclient
)

# -------------------

target_precompile_headers(tinychat_server PRIVATE src/pch.hpp)

target_include_directories(tinychat_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tinychat_server PRIVATE
    #nlohmann_json::nlohmann_json
    Boost::system
    Boost::json
    libmysqlclient::libmysqlclient
)

target_compile_definitions(tinychat_server PRIVATE
    "SEMAPHORE_MAX_VALUE=${SEMAPHORE_MAX_VALUE}"
)

target_compile_definitions(test_client PRIVATE
    "SEMAPHORE_MAX_VALUE=${SEMAPHORE_MAX_VALUE}"
)
