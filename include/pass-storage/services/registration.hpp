#pragma once

#include <memory>
#include <optional>
#include <string>
#include <pqxx/pqxx>

class Registration {
public:
    explicit Registration(std::shared_ptr<pqxx::connection> db_connection);

    std::optional<int> registration() const;

private:
    std::shared_ptr<pqxx::connection> db_;
    std::optional<int> save_user(std::string email, std::string password, std::string username, const std::string& questions_json_str, std::string phone) const;
    std::string adapt_questions() const;
};