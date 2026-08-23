#include "pass-storage/services/auth.hpp"
#include "pass-storage/services/otp.hpp"
#include "pass-storage/crypto/hashing.hpp"
#include <random>
#include "pass-storage/crypto/master_key.hpp"
#include <fmt/ostream.h>
#include <iostream>
#include <nlohmann/json.hpp>

Auth::Auth(std::shared_ptr<pqxx::connection> db_connection)
    : db_(std::move(db_connection)) {}

bool Auth::authenticate_email(const std::string& email) const {
    pqxx::work tx(*db_);
    const pqxx::result result = tx.exec("SELECT 1 FROM users WHERE email = $1", pqxx::params{email});
    tx.commit();

    return !result.empty();
}

bool Auth::authenticate_password(const std::string& email, const std::string& user_password) const {
    pqxx::work tx(*db_);

    const pqxx::result result = tx.exec("SELECT password_hash, salt FROM users WHERE email = $1", pqxx::params{email});
    tx.commit();

    if (!result.empty()) {
        const auto password_hash = result[0][0].as<std::string>();
        const auto salt = result[0][1].as<std::string>();
        const bool input_hash = Hashing::verify_data(user_password, password_hash, salt);

        return input_hash;
    }

    return false;
}

bool Auth::authenticate_otp(int user_id, const std::string& user_otp) const {
    pqxx::work tx(*db_);

    const pqxx::result result = tx.exec("SELECT otp_code FROM users WHERE id = $1", pqxx::params{user_id});
    tx.commit();

    if (!result.empty()) {
        const auto db_otp = result[0][0].as<std::string>();

        return db_otp == user_otp;
    }

    return false;
}

std::tuple<std::string, std::string, std::string> Auth::get_question(int user_id) const {
    pqxx::work tx(*db_);

    const pqxx::result result = tx.exec("SELECT questions::text FROM users WHERE id = $1", pqxx::params{user_id});
    tx.commit();

    if (!result.empty()) {
        nlohmann::json questions = nlohmann::json::parse(result[0][0].as<std::string>());
        if (questions.empty()) throw std::runtime_error("User has no questions");

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dist(0, questions.size() - 1);

        auto it = questions.begin();
        std::advance(it, dist(gen));

        const auto& data = it.value();
        return {it.key(), data["hash"].get<std::string>(), data["salt"].get<std::string>()};
    }

    throw std::runtime_error("User not found");
}

std::optional<int> Auth::get_user_id(const std::string& email) const {
    pqxx::work tx(*db_);
    const pqxx::result result = tx.exec("SELECT id FROM users WHERE email = $1", pqxx::params{email});
    tx.commit();

    if (!result.empty()) {
        return result[0][0].as<int>();
    }

    return std::nullopt;
}

std::tuple<std::optional<int>, std::string, Botan::secure_vector<uint8_t>> Auth::basic_authentication() const {
    fmt::println("Enter your email: ");
    std::string email;
    std::cin >> email;
    if (!authenticate_email(email)) {
        fmt::println("Your email is not correct!");
        return {std::nullopt, "", {}};
    }

    fmt::println("Enter your password: ");
    std::string password;
    std::cin >> password;
    if (!authenticate_password(email, password)) {
        fmt::println("Your password is not correct!");
        return {std::nullopt, "", {}};
    }

    return {get_user_id(email), email, get_master_key(email, password)};
}

bool Auth::advanced_authentication(int user_id) const {
    fmt::println("Enter your OTP on the email: ");
    std::string otp;
    std::cin >> otp;
    if (!authenticate_otp(user_id, otp)) {
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

Botan::secure_vector<uint8_t> Auth::get_master_key(const std::string& email, const std::string& password) const {
    pqxx::work tx(*db_);

    const pqxx::result result = tx.exec("SELECT salt FROM users WHERE email = $1", pqxx::params{email});
    tx.commit();

    if (result.empty()) {
        return Botan::secure_vector<uint8_t>{};
    }

    const auto salt = result[0][0].as<std::string>();
    std::span<const uint8_t> salt_span(reinterpret_cast<const uint8_t*>(salt.data()), salt.size());

    return Master_Key_Generator::derive_key(password, salt_span);
}