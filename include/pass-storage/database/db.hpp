#pragma once

#include <memory>
#include <pqxx/pqxx>

class DB_Manager {
public:
    DB_Manager();
    
    [[nodiscard]] std::shared_ptr<pqxx::connection> get_connection() const;

private:
    std::shared_ptr<pqxx::connection> connection_;
};