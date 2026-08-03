#pragma once

#include <string>
#include <memory>
#include <vector>
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

    bool append_folder(int user_id, const Data_Path& path);
    bool append_data(int user_id, const Data_Path& entry);
    std::string get_data(int user_id, const Data_Path& path);
    bool delete_data(int user_id, const Data_Path& path);
    bool update_data(int user_id, const Data_Path& path, const std::string& data);
    bool append_file(int user_id, const Data_Path& data_struct, const std::string& raw_file_path);
    static Data_Path adapt_path(const std::string& path);

private:
    std::shared_ptr<pqxx::connection> db_;
    static std::vector<std::string> build_json_path(const Data_Path& path);
};