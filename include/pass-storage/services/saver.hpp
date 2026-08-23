#pragma once

#include "pass-storage/services/data.hpp"
#include <string>
#include <memory>
#include <span>
#include <filesystem>
#include <pqxx/pqxx>

class Save_Manager {
public:
    explicit Save_Manager(std::shared_ptr<pqxx::connection> db_connection);
    std::string save_data(
        int user_id,
        const std::string& data,
        const Data_Manager::Data_Path& path,
        std::span<const unsigned char> master_key) const;

private:
    std::shared_ptr<pqxx::connection> db_;
    bool get_file(
        int user_id,
        const std::string& file_id,
        const std::filesystem::path& destination_path,
        std::span<const unsigned char> master_key,
        const std::string& path_context) const;
};