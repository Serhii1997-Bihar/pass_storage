cmake_minimum_required(VERSION 3.28)
project(pass_storage)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG 7.10.0
        SYSTEM
        EXCLUDE_FROM_ALL
)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 10.2.1
        SYSTEM
        EXCLUDE_FROM_ALL
)

FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
)

FetchContent_Declare(
        curl
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG curl-8_6_0
        SYSTEM
        EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(
        libpqxx
        fmt
        json
        curl
)

find_package(PostgreSQL REQUIRED)

get_filename_component(POSTGRES_LIB_DIR "${PostgreSQL_LIBRARY}" DIRECTORY)
set(POSTGRES_BIN_DIR "${POSTGRES_LIB_DIR}/../bin")

if (NOT OPENSSL_ROOT_DIR)

    set(OPENSSL_ROOT_DIR "${POSTGRES_LIB_DIR}/..")

endif()

find_package(OpenSSL REQUIRED)

find_program(
        SEVEN_ZIP_EXECUTABLE
        NAMES 7z 7za 7z.exe
        PATHS
        "C:/Program Files/7-Zip"
        "C:/Program Files (x86)/7-Zip"
)

if (NOT SEVEN_ZIP_EXECUTABLE)

    set(SEVEN_ZIP_EXECUTABLE "7z")

endif()

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/config.hpp"
)

add_executable(
        pass_storage
        src/main.cpp
        src/database/db.cpp
        src/services/auth.cpp
        src/helpers/text.cpp
        src/services/registration.cpp
        src/crypto/hashing.cpp
        include/pass-storage/models/user.hpp
        include/pass-storage/models/data.hpp
        src/services/data.cpp
        src/user/user.cpp
        src/services/otp.cpp
        src/helpers/load_env.cpp
        src/services/saver.cpp
        src/helpers/uuid.cpp
)

get_filename_component(POSTGRES_LIB_DIR "${PostgreSQL_LIBRARY}" DIRECTORY)
set(POSTGRES_BIN_DIR "${POSTGRES_LIB_DIR}/../bin")

target_link_libraries(
        pass_storage
        PRIVATE
        OpenSSL::Crypto
        pqxx
        fmt::fmt
        nlohmann_json::nlohmann_json
        "${POSTGRES_LIB_DIR}/libpq.lib"
        libcurl
        ws2_32
        secur32
)

target_include_directories(
        pass_storage
        PRIVATE
        include
        "${PostgreSQL_INCLUDE_DIRS}"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

add_custom_command(
        TARGET pass_storage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${POSTGRES_BIN_DIR}/libpq.dll"
        "${POSTGRES_BIN_DIR}/libcrypto-3-x64.dll"
        "${POSTGRES_BIN_DIR}/libssl-3-x64.dll"
        "${POSTGRES_BIN_DIR}/libintl-9.dll"
        "${POSTGRES_BIN_DIR}/libiconv-2.dll"
        $<TARGET_FILE_DIR:pass_storage>
)

if (MINGW)
    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    add_custom_command(
            TARGET pass_storage POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MINGW_BIN_DIR}/libwinpthread-1.dll"
            "${MINGW_BIN_DIR}/libstdc++-6.dll"
            "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
            $<TARGET_FILE_DIR:pass_storage>
    )
endif()