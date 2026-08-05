#include "pass-storage/services/auth.hpp"
#include "pass-storage/services/otp.hpp"
#include "pass-storage/crypto/hashing.hpp"
#include <random>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <iostream>
#include <nlohmann/json.hpp>

Auth::Auth(std::shared_ptr<pqxx::connection> db_connection)
    : db_(std::move(db_connection)) {}

bool Auth::authenticate_email(const std::string& email) const {
    pqxx::work tx(*db_);
    const pqxx::result result = tx.exec_params("SELECT 1 FROM users WHERE email = $1", email);
    tx.commit();

    return !result.empty();
}

bool Auth::authenticate_password(const std::string& email, const std::string& user_password) const {
    pqxx::work tx(*db_);

    const pqxx::result result = tx.exec_params("SELECT password_hash, salt FROM users WHERE email = $1", email);
    tx.commit();

    if (!result.empty()) {
        const std::string password_hash = result[0][0].as<std::string>();

        const std::string salt = result[0][1].as<std::string>();
        bool input_hash = Hashing::verify_data(user_password, password_hash, salt);

        return input_hash;
    }

    return false;
}

bool Auth::authenticate_otp(int user_id, const std::string& user_otp) const {
    pqxx::work tx(*db_);
    const pqxx::result result = tx.exec_params("SELECT otp_code FROM users WHERE id = $1", user_id);
    tx.commit();

    if (!result.empty()) {
        const std::string db_otp = result[0][0].as<std::string>();
        return db_otp == user_otp;
    }

    return false;
}

std::tuple<std::string, std::string, std::string> Auth::get_question(int user_id) const {
    pqxx::work tx(*db_);
    const pqxx::result result = tx.exec_params("SELECT questions::text FROM users WHERE id = $1", user_id);
    tx.commit();

    if (result.empty()) {
        return {"", "", ""};
    }

    nlohmann::json questions = nlohmann::json::parse(result[0][0].as<std::string>());
    if (questions.empty()) {
        return {"", "", ""};
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, questions.size() - 1);

    auto it = questions.begin();
    std::advance(it, dist(gen));

    const auto& data = it.value();
    return {
        it.key(),
        data["hash"].get<std::string>(),
        data["salt"].get<std::string>()
    };
}

std::optional<int> Auth::get_user_id(const std::string& email) const {
    pqxx::work tx(*db_);
    const pqxx::result result = tx.exec_params("SELECT id FROM users WHERE email = $1", email);
    tx.commit();

    if (!result.empty()) {
        return result[0][0].as<int>();
    }
    return std::nullopt;
}

std::pair<std::optional<int>, std::string> Auth::basic_authentication() const {
    fmt::println("Enter your email: ");
    std::string email;
    std::cin >> email;
    bool is_email = authenticate_email(email);
    if (!is_email) {
        fmt::println("Your email is not correct!");
        return {std::nullopt, ""};
    }

    fmt::println("Enter your password: ");
    std::string password;
    std::cin >> password;
    bool is_password = authenticate_password(email, password);
    if (!is_password) {
        fmt::println("Your password is not correct!");
        return {std::nullopt, ""};
    }

    return { get_user_id(email), email };
}

bool Auth::advanced_authentication(int user_id) const {
    fmt::println("Enter your OTP on the email: ");
    std::string otp;
    std::cin >> otp;
    bool is_otp = authenticate_otp(user_id, otp);
    if (!is_otp) {
        fmt::println("Your otp is not correct!");
        return false;
    }

    auto [question, expected_hash, salt] = get_question(user_id);
    if (question.empty()) {
        fmt::println("No security questions found for this user!");
        return false;
    }

    fmt::println("{}", question);
    std::string answer;
    std::getline(std::cin >> std::ws, answer);

    if (!Hashing::verify_data(answer, expected_hash, salt)) {
        fmt::println("Your answer is not correct!");
        return false;
    }

    fmt::println("You was authenticated successfully ✅");
    fmt::println("======================================");
    fmt::println("");

    return true;
}
