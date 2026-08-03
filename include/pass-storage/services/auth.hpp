#pragma once

#include <string>
#include <memory>
#include <pqxx/pqxx>

class Auth {
public:
    explicit Auth(std::shared_ptr<pqxx::connection> db_connection);
    std::pair<std::optional<int>, std::string> basic_authentication() const;
    bool advanced_authentication(int user_id) const;

private:
    std::shared_ptr<pqxx::connection> db_;

    bool authenticate_email(const std::string& email) const;
    bool authenticate_password(const std::string& email, const std::string& user_password) const;

    bool authenticate_otp(int user_id, const std::string& user_otp) const;
    bool authenticate_answer(int user_id, const std::string& user_answer) const;

    std::optional<int> get_user_id(const std::string& email) const;
    std::pair<std::string, std::string> get_question(int user_id) const;
};