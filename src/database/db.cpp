#include "pass-storage/database/db.hpp"
#include "pass-storage/helpers/load_env.hpp"
#include <iostream>
#include <string>
#include <format>
#include <print>

static void create_tables_if_not_exists(const std::shared_ptr<pqxx::connection>& connection) {
    pqxx::work tx(*connection);

    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id SERIAL PRIMARY KEY,
            email VARCHAR(255) UNIQUE NOT NULL,
            password_hash VARCHAR(255) NOT NULL,
            salt VARCHAR(50) NOT NULL,
            otp_code VARCHAR(6),
            questions JSONB NOT NULL,
            username VARCHAR(255) NOT NULL,
            phone VARCHAR(30) NOT NULL
        );
    )");

    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS data (
            id SERIAL PRIMARY KEY,
            user_id INT UNIQUE NOT NULL,
            data JSONB NOT NULL,
            CONSTRAINT fk_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            CONSTRAINT check_mandatory_fields CHECK (
                data ? 'text' AND
                data ? 'files' AND
                data ? 'documents'
            )
        );
    )");

    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS file_storage (
            file_id UUID PRIMARY KEY,
            user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            content BYTEA NOT NULL,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
        );
    )");
    tx.commit();
}

DB_Manager::DB_Manager() {
    try {
        load_env();

        std::string host = get_env_var("DB_HOST", "");
        std::string port = get_env_var("DB_PORT", "");
        std::string dbname = get_env_var("DB_NAME", "");
        std::string user = get_env_var("DB_USER", "");
        std::string password = get_env_var("DB_PASSWORD", "");

        std::string conn_str = std::format("host={} port={} dbname={} user={} password={}", host, port, dbname, user, password);
        connection_ = std::make_shared<pqxx::connection>(conn_str);

        if (connection_->is_open()) {
            connection_->set_client_encoding("UTF8");
            std::println("Successfully connected to {} on the host {} 🟢", connection_->dbname(), host);
            create_tables_if_not_exists(connection_);
        }
    } catch (const std::exception& e) {
        std::cerr << "Database initialization failed: " << e.what() << "🔴\n";
        throw;
    }
}

std::shared_ptr<pqxx::connection> DB_Manager::get_connection() const {
    return connection_;
}