#include "pass-storage/services/data.hpp"
#include "pass-storage/helpers/uuid.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <fmt/core.h>

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

std::vector<std::string> Data_Manager::build_json_path(const Data_Path& path) {
    std::vector<std::string> json_path;
    if (!path.type_folder.empty()) json_path.push_back(path.type_folder);
    if (!path.name_folder.empty()) json_path.push_back(path.name_folder);
    if (!path.key.empty()) json_path.push_back(path.key);
    return json_path;
}

bool Data_Manager::append_folder(int user_id, const Data_Path& path) {
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

bool Data_Manager::append_data(int user_id, const Data_Path& data_struct) {
    try {
        pqxx::work tx(*db_);

        const auto new_data = nlohmann::json::object({ {data_struct.key, data_struct.value} });

        const auto result = tx.exec(
            "UPDATE data SET data = jsonb_set(data, array[$2, $3], "
            "COALESCE(data->$2->$3, '[]'::jsonb) || $4::jsonb, true) WHERE user_id = $1",
            pqxx::params{user_id, data_struct.type_folder, data_struct.name_folder, new_data.dump()}
        );

        tx.commit();
        return result.affected_rows() > 0;
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in append_data: {}", e.what());
        return false;
    }
}

std::string Data_Manager::get_data(int user_id, const Data_Path& path) {
    try {
        pqxx::read_transaction tx(*db_);
        const std::vector<std::string> json_path = build_json_path(path);

        const pqxx::result result = tx.exec(
            "SELECT (data #>> $2) FROM data WHERE user_id = $1",
            pqxx::params{user_id, json_path}
        );

        if (!result.empty() && !result[0][0].is_null()) {
            return result[0][0].as<std::string>();
        }
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in get_data: {}", e.what());
    }

    return "";
}

bool Data_Manager::delete_data(int user_id, const Data_Path& path) {
    try {
        pqxx::work tx(*db_);
        const std::vector<std::string> json_path = build_json_path(path);

        const pqxx::result result = tx.exec(
            "UPDATE data SET data = data #- $2 WHERE user_id = $1 AND data #> $2 IS NOT NULL",
            pqxx::params{user_id, json_path}
        );
        tx.commit();

        return result.affected_rows() > 0;
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in delete_data: {}", e.what());
        return false;
    }
}

bool Data_Manager::update_data(int user_id, const Data_Path& path, const std::string& new_value) {
    try {
        const std::vector<std::string> json_path = build_json_path(path);
        if (json_path.size() != 3) return false;

        pqxx::work tx(*db_);

        const pqxx::result result = tx.exec(
            "UPDATE data SET data = jsonb_set(data, $2, $3::jsonb, false) "
            "WHERE user_id = $1 AND data #> $2 IS NOT NULL",
            pqxx::params{user_id, json_path, nlohmann::json(new_value).dump()}
        );
        tx.commit();

        return result.affected_rows() > 0;
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in update_data: {}", e.what());
        return false;
    }
}

bool Data_Manager::append_file(int user_id, const Data_Path& data_struct, const std::string& raw_file_path) {
    if (data_struct.type_folder.empty() || data_struct.name_folder.empty() || data_struct.key.empty()) return false;

    std::string clean_path_str = raw_file_path;
    if (clean_path_str.size() >= 2 && clean_path_str.front() == '"' && clean_path_str.back() == '"') {
        clean_path_str = clean_path_str.substr(1, clean_path_str.size() - 2);
    }

    const std::filesystem::path file_path = std::filesystem::path(clean_path_str).lexically_normal();

    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    const auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(file_size);
    if (!file.read(buffer.data(), file_size)) return false;

    const std::string file_id = generate_uuid_v4();

    try {
        pqxx::work tx(*db_);

        const auto* data_ptr = reinterpret_cast<const std::byte*>(buffer.data());

        tx.exec(
            "INSERT INTO file_storage (file_id, user_id, content) VALUES ($1::uuid, $2, $3)",
            pqxx::params{file_id, user_id, pqxx::bytes(data_ptr, data_ptr + buffer.size())}
        );

        const auto new_file_entry = nlohmann::json::object({ {data_struct.key, file_id} });
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