#pragma once

#include <string>
#include <memory>
#include <filesystem>
#include <pqxx/pqxx>
#include "pass-storage/services/data.hpp"

class Save_Manager {
public:
    explicit Save_Manager(std::shared_ptr<pqxx::connection> db_connection);

    std::string save_data(int user_id, const std::string& data, const Data_Manager::Data_Path& path);

private:
    std::shared_ptr<pqxx::connection> db_;

    bool get_file(int user_id, const std::string& file_id, const std::filesystem::path& destination_path);
};