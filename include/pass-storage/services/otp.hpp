#pragma once

#include <memory>
#include <pqxx/pqxx>

class Otp_Manager {
public:
    explicit Otp_Manager(std::shared_ptr<pqxx::connection> db_connection);

    [[nodiscard]] int get_otp(int user_id) const;
    [[nodiscard]] bool update_otp(int user_id);

private:
    std::shared_ptr<pqxx::connection> db_;

    static std::string generate_otp();
    [[nodiscard]] static bool send_otp(const std::string& user_email, const std::string& otp_code);
};