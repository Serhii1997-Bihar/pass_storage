#include "pass-storage/services/data.hpp"
#include "pass-storage/crypto/encryption.hpp"
#include "pass-storage/helpers/uuid.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <span>
#include <format>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <botan/base64.h>

Data_Manager::Data_Manager(std::shared_ptr<pqxx::connection> db_connection)
    : db_(std::move(db_connection)) {}

Data_Manager::Data_Path Data_Manager::adapt_path(const std::string& path) {
    Data_Path result{};
    std::stringstream ss(path);
    std::string segment;
    std::vector<std::string> segments;

    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }

    if (segments.size() >= 1) result.type_folder = segments[0];
    if (segments.size() >= 2) result.name_folder = segments[1];
    if (segments.size() >= 3) result.key = segments[2];

    return result;
}

std::string Data_Manager::build_path_context(const Data_Path& path) {
    return std::format("{}/{}/{}", path.type_folder, path.name_folder, path.key);
}

bool Data_Manager::append_folder(int user_id, const Data_Path& path) const {
    try {
        pqxx::work tx(*db_);

        tx.exec(
            "INSERT INTO data (user_id, data) "
            "VALUES ($1, '{\"text\": {}, \"files\": {}, \"documents\": {}}'::jsonb) "
            "ON CONFLICT (user_id) DO NOTHING",
            pqxx::params{user_id}
        );

        const pqxx::result result = tx.exec(
            "UPDATE data "
            "SET data = jsonb_set(data, array[$2, $3], '{}'::jsonb, true) "
            "WHERE user_id = $1 AND NOT (data->$2 ? $3)",
            pqxx::params{user_id, path.type_folder, path.name_folder}
        );
        tx.commit();

        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in append_folder: {}", e.what());
        return false;
    }
}

bool Data_Manager::append_data(int user_id, const Data_Path& entry, const std::span<const unsigned char> master_key) const {
    if (entry.type_folder.empty() || entry.name_folder.empty() || entry.key.empty()) return false;

    try {
        pqxx::work tx(*db_);

        const std::string path_context = build_path_context(entry);
        const std::span<const unsigned char> text_span(reinterpret_cast<const unsigned char*>(entry.value.data()), entry.value.size());

        const auto encrypted_data = Encryption::encrypt(text_span, master_key, path_context);
        std::string b64_encrypted = Botan::base64_encode(encrypted_data.data(), encrypted_data.size());

        const auto new_data = nlohmann::json::object({ {entry.key, b64_encrypted} });

        const auto result = tx.exec(
            "UPDATE data SET data = jsonb_set(data, array[$2, $3], "
            "COALESCE(data->$2->$3, '{}'::jsonb) || $4::jsonb, true) WHERE user_id = $1",
            pqxx::params{user_id, entry.type_folder, entry.name_folder, new_data.dump()}
        );
        tx.commit();

        return result.affected_rows() > 0;
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in append_data: {}", e.what());
        return false;
    }
}

std::string Data_Manager::get_data(int user_id, const Data_Path& path, const std::span<const unsigned char> master_key) const {
    try {
        pqxx::read_transaction tx(*db_);
        pqxx::result result;

        if (path.key.empty()) {
            result = tx.exec(
                "SELECT (data #> array[$2, $3])::text FROM data WHERE user_id = $1",
                pqxx::params{user_id, path.type_folder, path.name_folder}
            );

            if (!result.empty() && !result[0][0].is_null()) {
                return result[0][0].as<std::string>();
            }

        } else {
            result = tx.exec(
                "SELECT (data #>> array[$2, $3, $4]) FROM data WHERE user_id = $1",
                pqxx::params{user_id, path.type_folder, path.name_folder, path.key}
            );

            if (!result.empty() && !result[0][0].is_null()) {
                const auto data = result[0][0].as<std::string>();

                if (path.type_folder == "files") return data;

                auto encrypted_data = Botan::base64_decode(data);
                const std::string path_context = build_path_context(path);
                const std::span<const unsigned char> enc_span(encrypted_data.data(), encrypted_data.size());

                auto decrypted_data = Encryption::decrypt(enc_span, master_key, path_context);
                return {decrypted_data.begin(), decrypted_data.end()};
            }
        }
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in get_data: {}", e.what());
    }

    return "";
}

bool Data_Manager::delete_data(int user_id, const Data_Path& path) const {
    try {
        pqxx::work tx(*db_);
        pqxx::result result;

        if (path.key.empty()) {
            result = tx.exec(
                "UPDATE data SET data = data #- array[$2, $3] WHERE user_id = $1 AND data #> array[$2, $3] IS NOT NULL",
                pqxx::params{user_id, path.type_folder, path.name_folder}
            );
        } else {
            result = tx.exec(
                "UPDATE data SET data = data #- array[$2, $3, $4] WHERE user_id = $1 AND data #> array[$2, $3, $4] IS NOT NULL",
                pqxx::params{user_id, path.type_folder, path.name_folder, path.key}
            );
        }
        tx.commit();

        return result.affected_rows() > 0;
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in delete_data: {}", e.what());
        return false;
    }
}

bool Data_Manager::append_file(int user_id, const Data_Path& data_struct, const std::string& raw_file_path, std::span<const unsigned char> master_key) const {
    if (data_struct.type_folder.empty() || data_struct.name_folder.empty() || data_struct.key.empty()) return false;

    std::string clean_path_str = raw_file_path;
    if (clean_path_str.size() >= 2 && clean_path_str.front() == '"' && clean_path_str.back() == '"') {
        clean_path_str = clean_path_str.substr(1, clean_path_str.size() - 2);
    }

    const std::filesystem::path file_path = std::filesystem::path(clean_path_str).lexically_normal();

    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    const auto file_size = file.tellg();
    if (file_size <= 0) return false;
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(file_size);
    if (!file.read(buffer.data(), file_size)) return false;

    const std::string file_id = generate_uuid_v4();
    const std::string file_id_with_ext = file_id + file_path.extension().string();

    try {
        pqxx::work tx(*db_);

        const std::string path_context = build_path_context(data_struct);
        std::span<const unsigned char> file_span(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size());

        auto encrypted_data = Encryption::encrypt(file_span, master_key, path_context);
        const auto* data_ptr = reinterpret_cast<const std::byte*>(encrypted_data.data());

        tx.exec(
            "INSERT INTO file_storage (file_id, user_id, content) VALUES ($1::uuid, $2, $3)",
            pqxx::params{file_id, user_id, pqxx::bytes(data_ptr, data_ptr + encrypted_data.size())}
        );

        const auto new_file_entry = nlohmann::json::object({ {data_struct.key, file_id_with_ext} });
        const auto result = tx.exec(
            "UPDATE data SET data = jsonb_set(data, array[$2, $3], "
            "COALESCE(data->$2->$3, '{}'::jsonb) || $4::jsonb, true) WHERE user_id = $1",
            pqxx::params{user_id, data_struct.type_folder, data_struct.name_folder, new_file_entry.dump()}
        );

        tx.commit();
        return result.affected_rows() > 0;
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in append_file: {}", e.what());
        return false;
    }
}