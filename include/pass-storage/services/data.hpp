#pragma once

#include <string>
#include <memory>
#include <vector>
#include <span>
#include <pqxx/pqxx>
#include <filesystem>

class Data_Manager {
public:
    struct Data_Path {
        std::string type_folder;
        std::string name_folder;
        std::string key;
        std::string value;
    };

    explicit Data_Manager(std::shared_ptr<pqxx::connection> db_connection);

    bool append_folder(int user_id, const Data_Path& path) const;
    bool append_data(int user_id, const Data_Path& entry, std::span<const unsigned char> master_key) const;
    std::string get_data(int user_id, const Data_Path& path, std::span<const unsigned char> master_key) const;
    bool delete_data(int user_id, const Data_Path& path) const;
    bool append_file(int user_id, const Data_Path& data_struct, const std::string& raw_file_path, std::span<const unsigned char> master_key) const ;
    static Data_Path adapt_path(const std::string& path);

private:
    std::shared_ptr<pqxx::connection> db_;
    static std::string build_path_context(const Data_Path& path);
};