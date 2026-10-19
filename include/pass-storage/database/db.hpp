#pragma once

#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include <pqxx/pqxx>

class DB_Manager {
public:
    explicit DB_Manager(std::chrono::seconds timeout = std::chrono::seconds(120));
    ~DB_Manager();

    [[nodiscard]] std::shared_ptr<pqxx::connection> get_connection() const;
    void update_activity() const noexcept;

private:
    void monitor_loop(std::stop_token stop_token) const;

    std::shared_ptr<pqxx::connection> connection_;
    std::chrono::seconds timeout_;
    mutable std::atomic<std::chrono::steady_clock::time_point> last_activity_;
    std::jthread monitor_thread_;
};