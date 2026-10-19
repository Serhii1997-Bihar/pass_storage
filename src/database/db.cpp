#include "pass-storage/database/db.hpp"
#include "pass-storage/helpers/load_env.hpp"
#include <iostream>
#include <string>
#include <format>
#include <fmt/ostream.h>

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

DB_Manager::DB_Manager(std::chrono::seconds timeout)
    : timeout_(timeout),
      last_activity_(std::chrono::steady_clock::now()) {
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
            fmt::println("Successfully connected to {} on the host {} 🟢", connection_->dbname(), host);
            create_tables_if_not_exists(connection_);
        }

        monitor_thread_ = std::jthread([this](std::stop_token stop_token) {
            monitor_loop(stop_token);
        });
    } catch (const std::exception& e) {
        std::cerr << "Database initialization failed: " << e.what() << "🔴\n";
        throw;
    }
}

DB_Manager::~DB_Manager() {
    if (monitor_thread_.joinable()) {
        monitor_thread_.request_stop();
    }
}

std::shared_ptr<pqxx::connection> DB_Manager::get_connection() const {
    update_activity();
    return connection_;
}

void DB_Manager::update_activity() const noexcept {
    last_activity_.store(std::chrono::steady_clock::now());
}

void DB_Manager::monitor_loop(std::stop_token stop_token) const {
    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto now = std::chrono::steady_clock::now();
        if ((now - last_activity_.load()) >= timeout_) {
            fmt::println("\n[Security] No activity for 2 minutes. Auto-closing pass_storage... 🔴");
            std::exit(0);
        }
    }
}