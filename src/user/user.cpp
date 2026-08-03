#include "pass-storage/user/user.hpp"
#include "pass-storage/services/data.hpp"
#include <nlohmann/json.hpp>
#include <fmt/ostream.h>

User_Manager::User_Manager(std::shared_ptr<pqxx::connection> db_connection)
    : db_(std::move(db_connection)) {}

bool User_Manager::update_data(int user_id, const Verify_Object& data_object, std::string data) const {
    pqxx::work tx(*db_);
    pqxx::result result;

    if (data_object == Verify_Object::Password) {
        result = tx.exec_params(
            "UPDATE users SET password = $1 WHERE user_id = $2", data, user_id
        );
    } else if (data_object == Verify_Object::Email) {
        result = tx.exec_params(
            "UPDATE users SET email = $1 WHERE user_id = $2", data, user_id
        );
    }

    tx.commit();
    return result.affected_rows() > 0;
}

std::string User_Manager::get_username(int user_id) const {
    pqxx::work tx(*db_);

    pqxx::result result = tx.exec_params("SELECT username FROM users WHERE id = $1", user_id);
    tx.commit();

    if (!result.empty()) {
        return result[0][0].as<std::string>();
    }

    return "Undefined";
}