#pragma once

#include <string>
#include <memory>
#include <pqxx/pqxx>

class User_Manager {
public:
    explicit User_Manager(std::shared_ptr<pqxx::connection> db_connection);

    enum class Verify_Object {
        Email,
        Password
    };

    bool update_data(int user_id, const Verify_Object& data_object, std::string data) const;
    std::string get_username(int user_id) const;

private:
    std::shared_ptr<pqxx::connection> db_;
};