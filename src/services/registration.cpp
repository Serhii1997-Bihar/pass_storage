#include "pass-storage/services/registration.hpp"
#include "pass-storage/crypto/hashing.hpp"
#include "pass-storage/models/user.hpp"
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <limits>

Registration::Registration(std::shared_ptr<pqxx::connection> db_connection)
    : db_(std::move(db_connection)) {}

std::optional<int> Registration::save_user(
    std::string_view email,
    std::string_view password,
    std::string_view username,
    std::string_view questions_json_str,
    std::string_view phone
) const {
    pqxx::work tx(*db_);

    auto [hashed_password, salt] = Hashing::get_data_and_salt(password);

    const pqxx::result result = tx.exec(
        "INSERT INTO users (email, password_hash, salt, username, questions, phone) "
        "VALUES ($1, $2, $3, $4, $5::jsonb, $6) RETURNING id",
        pqxx::params{email, hashed_password, salt, username, questions_json_str, phone}
    );

    if (result.empty()) {
        return std::nullopt;
    }

    int user_id = result[0]["id"].as<int>();

    tx.exec(
        "INSERT INTO data (user_id, data) "
        "VALUES ($1, '{\"text\": {}, \"files\": {}, \"documents\": {}}'::jsonb)",
        pqxx::params{user_id}
    );

    tx.commit();
    return user_id;
}

std::string Registration::adapt_questions() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    nlohmann::json questions_json = nlohmann::json::object();
    fmt::println("You need to put your 10 questions and answers below for security");

    while (questions_json.size() < 10) {
        fmt::println("[{}/10] Enter your question: ", questions_json.size() + 1);
        std::string question;
        std::getline(std::cin >> std::ws, question);

        fmt::println("Enter your answer: ");
        std::string answer;
        std::getline(std::cin >> std::ws, answer);

        if (question.empty() || answer.empty()) {
            fmt::println("Question or answer cannot be empty. Try again.");
            continue;
        }

        auto [hashed_answer, answer_salt] = Hashing::get_data_and_salt(answer);

        questions_json[question] = {
            {"hash", hashed_answer},
            {"salt", answer_salt}
        };
    }

    return questions_json.dump();
}

std::optional<int> Registration::registration() const {
    fmt::println("Enter your username: ");
    std::string username;
    std::cin >> username;

    fmt::println("Enter your email: ");
    std::string email;
    std::cin >> email;

    fmt::println("Enter your password: ");
    std::string password;
    std::cin >> password;

    fmt::println("Enter your phone: ");
    std::string phone;
    std::cin >> phone;

    const std::string questions_json_str = adapt_questions();

    std::optional<int> user_id = save_user(email, password, username, questions_json_str, phone);
    if (user_id.has_value()) {
        fmt::println("Your account has been created successfully ✅");
        fmt::println("======================================");
        fmt::println("");
        return user_id;
    }

    fmt::println("Something went wrong. Please try again.");
    return std::nullopt;
}