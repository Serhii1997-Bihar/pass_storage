#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <pqxx/pqxx>

class Registration {
public:
    explicit Registration(std::shared_ptr<pqxx::connection> db_connection);

    std::optional<int> registration() const;

private:
    std::shared_ptr<pqxx::connection> db_;

    std::optional<int> save_user(
        std::string_view email,
        std::string_view password,
        std::string_view username,
        std::string_view questions_json_str,
        std::string_view phone
    ) const;

    static std::string adapt_questions();
};