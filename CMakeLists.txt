cmake_minimum_required(VERSION 3.28)
set(VCPKG_MANIFEST_INSTALL_OPTIONS "--no-binarycaching")
project(pass_storage)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release)

find_package(libpqxx CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Botan CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

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
        src/crypto/encryption.cpp
        include/pass-storage/models/user.hpp
        include/pass-storage/models/data.hpp
        src/services/data.cpp
        src/user/user.cpp
        src/services/otp.cpp
        src/helpers/load_env.cpp
        src/services/saver.cpp
        src/helpers/uuid.cpp
)

target_link_libraries(
        pass_storage
        PRIVATE
        OpenSSL::Crypto
        OpenSSL::SSL
        libpqxx::pqxx
        fmt::fmt
        nlohmann_json::nlohmann_json
        ${PostgreSQL_LIBRARIES}
        CURL::libcurl
        ws2_32
        secur32
        crypt32
        wldap32
        Botan::Botan
        Threads::Threads
)

if (MSVC AND MSVC_VERSION GREATER_EQUAL 1950)
    target_link_options(pass_storage PRIVATE /FORCE:MULTIPLE)
endif()

target_include_directories(
        pass_storage
        PRIVATE
        include
        "${PostgreSQL_INCLUDE_DIRS}"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
