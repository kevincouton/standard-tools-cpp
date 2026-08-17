cmake_minimum_required(VERSION 3.25)
project(standard-tools-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STANDARD_TOOLS_ENABLE_GRPC "Build gRPC health server and tools" ON)
option(STANDARD_TOOLS_ENABLE_POSTGRES "Build PostgreSQL persistence" ON)
option(STANDARD_TOOLS_ENABLE_CURL "Build healthcheck with HTTP support" ON)
option(STANDARD_TOOLS_BUILD_INTEGRATION_TESTS "Build integration tests" OFF)
option(STANDARD_TOOLS_BUILD_E2E_TESTS "Build end-to-end tests" OFF)

include(FetchContent)

# Header-only dependencies fetched from source.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.0
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.0
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    date
    GIT_REPOSITORY https://github.com/HowardHinnant/date.git
    GIT_TAG v3.0.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(
    nlohmann_json
    Catch2
    CLI11
    tomlplusplus
    date
)

FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include CACHE PATH "asio include dir" FORCE)

FetchContent_MakeAvailable(Crow)

# System dependencies.
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

if(STANDARD_TOOLS_ENABLE_CURL)
    find_package(CURL)
    if(NOT CURL_FOUND)
        message(STATUS "libcurl not found; disabling HTTP healthcheck")
        set(STANDARD_TOOLS_ENABLE_CURL OFF)
    endif()
endif()

if(STANDARD_TOOLS_ENABLE_GRPC)
    find_package(gRPC QUIET)
    if(NOT gRPC_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(GRPC grpc++)
            pkg_check_modules(PROTOBUF protobuf)
        endif()
    endif()
    if(NOT gRPC_FOUND AND NOT GRPC_FOUND)
        message(STATUS "gRPC not found; disabling gRPC server and tools")
        set(STANDARD_TOOLS_ENABLE_GRPC OFF)
    endif()
endif()

if(STANDARD_TOOLS_ENABLE_POSTGRES)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBPQXX libpqxx>=7.0)
    endif()
    if(NOT LIBPQXX_FOUND)
        find_path(LIBPQXX_INCLUDE_DIRS pqxx/pqxx)
        find_library(LIBPQXX_LIBRARIES pqxx)
        if(LIBPQXX_INCLUDE_DIRS AND LIBPQXX_LIBRARIES)
            set(LIBPQXX_FOUND TRUE)
        endif()
    endif()
    if(NOT LIBPQXX_FOUND)
        message(STATUS "libpqxx not found; disabling PostgreSQL persistence")
        set(STANDARD_TOOLS_ENABLE_POSTGRES OFF)
    endif()
endif()

set(CORE_SOURCES
    src/standard_tools/core/value_objects.cpp
    src/standard_tools/audit/record.cpp
    src/standard_tools/audit/storage.cpp
    src/standard_tools/audit/writer.cpp
    src/standard_tools/audit/verifier.cpp
    src/standard_tools/audit/replay.cpp
    src/standard_tools/marketdata/cache.cpp
    src/standard_tools/marketdata/synthetic.cpp
    src/standard_tools/marketdata/service.cpp
    src/standard_tools/indicators/calculator.cpp
    src/standard_tools/backtest/strategy.cpp
    src/standard_tools/backtest/engine.cpp
    src/standard_tools/backtest/monte_carlo.cpp
    src/standard_tools/backtest/walk_forward.cpp
    src/standard_tools/agent/tool.cpp
    src/standard_tools/agent/dispatcher.cpp
    src/standard_tools/config/config.cpp
    src/standard_tools/metrics/risk_return.cpp
    src/standard_tools/analysis/calculator.cpp
    src/standard_tools/screener/criteria.cpp
    src/standard_tools/screener/hardcoded_provider.cpp
    src/standard_tools/screener/service.cpp
    src/standard_tools/portfolio/optimizer.cpp
    src/standard_tools/portfolio/risk_parity.cpp
    src/standard_tools/portfolio/black_litterman.cpp
)

set(API_SOURCES
    src/standard_tools/api/rest.cpp
    src/standard_tools/api/a2a.cpp
    src/standard_tools/api/mcp.cpp
)

if(STANDARD_TOOLS_ENABLE_POSTGRES)
    list(APPEND CORE_SOURCES
        src/standard_tools/storage/postgres.cpp
        src/standard_tools/storage/migrate.cpp
        src/standard_tools/audit/postgres.cpp
    )
endif()

if(STANDARD_TOOLS_ENABLE_GRPC)
    # Generate protobuf/gRPC sources.
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
    if(NOT GRPC_CPP_PLUGIN)
        message(FATAL_ERROR "grpc_cpp_plugin not found")
    endif()

    # Prefer protobuf CMake config, but fall back to the FindProtobuf module
    # (used by Ubuntu/Debian packages).
    find_package(protobuf CONFIG QUIET)
    if(NOT protobuf_FOUND AND NOT Protobuf_FOUND)
        find_package(Protobuf REQUIRED)
    endif()

    set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/health.proto)
    set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
    file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

    if(TARGET protobuf::protoc)
        set(PROTOC_EXECUTABLE protobuf::protoc)
    elseif(TARGET protobuf::libprotobuf)
        set(PROTOC_EXECUTABLE protobuf::protoc)
    else()
        find_program(PROTOC_EXECUTABLE protoc REQUIRED)
    endif()

    add_custom_command(
        OUTPUT
            ${PROTO_GEN_DIR}/health.pb.cc
            ${PROTO_GEN_DIR}/health.pb.h
            ${PROTO_GEN_DIR}/health.grpc.pb.cc
            ${PROTO_GEN_DIR}/health.grpc.pb.h
        COMMAND ${PROTOC_EXECUTABLE}
            --grpc_out=${PROTO_GEN_DIR}
            --cpp_out=${PROTO_GEN_DIR}
            -I ${CMAKE_CURRENT_SOURCE_DIR}/proto
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
    )

    add_library(standard_tools_proto STATIC
        ${PROTO_GEN_DIR}/health.pb.cc
        ${PROTO_GEN_DIR}/health.grpc.pb.cc
    )
    target_include_directories(standard_tools_proto PUBLIC ${PROTO_GEN_DIR})
    if(gRPC_FOUND)
        target_link_libraries(standard_tools_proto PUBLIC gRPC::grpc++ protobuf::libprotobuf)
    elseif(TARGET protobuf::libprotobuf)
        target_link_libraries(standard_tools_proto PUBLIC ${GRPC_LIBRARIES} protobuf::libprotobuf)
    else()
        target_include_directories(standard_tools_proto PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${GRPC_INCLUDE_DIRS})
        target_link_libraries(standard_tools_proto PUBLIC ${PROTOBUF_LIBRARIES} ${GRPC_LIBRARIES})
    endif()

    list(APPEND API_SOURCES src/standard_tools/api/grpc_server.cpp)
endif()

add_library(standard_tools_core STATIC ${CORE_SOURCES} ${API_SOURCES})
target_include_directories(standard_tools_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(standard_tools_core
    PUBLIC
        nlohmann_json::nlohmann_json
        Crow::Crow
        date::date
        tomlplusplus::tomlplusplus
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)

if(STANDARD_TOOLS_ENABLE_POSTGRES)
    target_compile_definitions(standard_tools_core PUBLIC STANDARD_TOOLS_ENABLE_POSTGRES=1)
    target_include_directories(standard_tools_core PUBLIC ${LIBPQXX_INCLUDE_DIRS})
    target_link_libraries(standard_tools_core PUBLIC ${LIBPQXX_LIBRARIES})
endif()

if(STANDARD_TOOLS_ENABLE_GRPC)
    target_compile_definitions(standard_tools_core PUBLIC STANDARD_TOOLS_ENABLE_GRPC=1)
    target_link_libraries(standard_tools_core PUBLIC standard_tools_proto)
endif()

# Server executable.
add_executable(server src/server/main.cpp)
target_link_libraries(server PRIVATE standard_tools_core)
if(STANDARD_TOOLS_ENABLE_GRPC)
    target_compile_definitions(server PRIVATE STANDARD_TOOLS_ENABLE_GRPC=1)
endif()
if(STANDARD_TOOLS_ENABLE_POSTGRES)
    target_compile_definitions(server PRIVATE STANDARD_TOOLS_ENABLE_POSTGRES=1)
endif()

# CLI executable.
add_executable(cli src/cli/main.cpp)
target_link_libraries(cli PRIVATE standard_tools_core CLI11::CLI11)
if(STANDARD_TOOLS_ENABLE_POSTGRES)
    target_compile_definitions(cli PRIVATE STANDARD_TOOLS_ENABLE_POSTGRES=1)
endif()

# Tools.
if(STANDARD_TOOLS_ENABLE_GRPC)
    add_executable(grpc_health_check src/tools/grpc_health_check/main.cpp)
    target_link_libraries(grpc_health_check PRIVATE standard_tools_proto)
endif()

if(STANDARD_TOOLS_ENABLE_CURL AND STANDARD_TOOLS_ENABLE_GRPC)
    add_executable(healthcheck src/tools/healthcheck/main.cpp)
    target_link_libraries(healthcheck PRIVATE standard_tools_proto CURL::libcurl)
elseif(STANDARD_TOOLS_ENABLE_GRPC)
    add_executable(healthcheck src/tools/healthcheck/main.cpp)
    target_link_libraries(healthcheck PRIVATE standard_tools_proto)
endif()

# Tests.
enable_testing()

set(UNIT_TEST_SOURCES
    tests/unit/core/test_value_objects.cpp
    tests/unit/marketdata/test_synthetic.cpp
    tests/unit/indicators/test_indicators.cpp
    tests/unit/audit/test_audit.cpp
    tests/unit/api/test_auth.cpp
    tests/unit/agent/test_dispatcher.cpp
    tests/unit/metrics/test_metrics.cpp
    tests/unit/analysis/test_analysis.cpp
    tests/unit/screener/test_screener.cpp
    tests/unit/backtest/test_backtest.cpp
    tests/unit/portfolio/test_portfolio.cpp
)

add_executable(unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE standard_tools_core Catch2::Catch2WithMain)
add_test(NAME unit_tests COMMAND unit_tests)

if(STANDARD_TOOLS_BUILD_INTEGRATION_TESTS)
    add_executable(integration_tests tests/integration/test_postgres.cpp)
    target_link_libraries(integration_tests PRIVATE standard_tools_core Catch2::Catch2WithMain)
    target_compile_definitions(integration_tests PRIVATE STANDARD_TOOLS_ENABLE_POSTGRES=1)
    add_test(NAME integration_tests COMMAND integration_tests)
    set_tests_properties(integration_tests PROPERTIES SKIP_RETURN_CODE 4)
endif()

if(STANDARD_TOOLS_BUILD_E2E_TESTS)
    add_executable(e2e_tests tests/e2e/test_server.cpp)
    target_link_libraries(e2e_tests PRIVATE standard_tools_core Catch2::Catch2WithMain)
    if(STANDARD_TOOLS_ENABLE_GRPC)
        target_link_libraries(e2e_tests PRIVATE standard_tools_proto)
    endif()
    if(STANDARD_TOOLS_ENABLE_CURL)
        target_link_libraries(e2e_tests PRIVATE CURL::libcurl)
    endif()
    add_test(NAME e2e_tests COMMAND e2e_tests)
endif()
