#include "pass-storage/services/otp.hpp"
#include "pass-storage/helpers/load_env.hpp"
#include <random>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <iostream>
#include <curl/curl.h>

Otp_Manager::Otp_Manager(std::shared_ptr<pqxx::connection> db_connection)
    : db_(std::move(db_connection)) {}

bool Otp_Manager::send_otp(const std::string& user_email, const std::string& otp_code) {
    struct Upload_Status {
        size_t bytes_read;
        std::string payload;
    };

    auto payload_source = [](char* ptr, size_t size, size_t nmemb, void* userp) -> size_t const {
        auto* upload_ctx = static_cast<Upload_Status*>(userp);
        size_t room = size * nmemb;

        if (room < 1 || upload_ctx->bytes_read >= upload_ctx->payload.size()) {
            return 0;
        }

        size_t len = std::min(room, upload_ctx->payload.size() - upload_ctx->bytes_read);
        std::memcpy(ptr, upload_ctx->payload.data() + upload_ctx->bytes_read, len);
        upload_ctx->bytes_read += len;

        return len;
    };

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string from = get_env_var("EMAIL_FROM", "");
    std::string password = get_env_var("EMAIL_PASSWORD", "");
    std::string smtp_url = get_env_var("SMTP_URL", "");

    std::string payload_text = fmt::format(
        "To: {}\r\n"
        "From: {}\r\n"
        "Subject: Your OTP Code\r\n"
        "\r\n"
        "Your verification code is: {}\r\n",
        user_email, from, otp_code
    );

    Upload_Status upload_ctx{ .bytes_read = 0, .payload = payload_text };

    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, user_email.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, smtp_url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME, from.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, +payload_source);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(payload_text.size()));
    curl_easy_setopt(curl, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) {
        return true;
    }

    fmt::println(stderr, "Failed to send OTP via cURL: {}", curl_easy_strerror(res));
    return false;
}

std::string Otp_Manager::generate_otp() {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(100000, 999999);

    return std::to_string(dist(gen));
}

bool Otp_Manager::update_otp(int user_id) {
    pqxx::work tx(*db_);
    std::string new_otp = generate_otp();

    pqxx::result result = tx.exec_params(
        "UPDATE users SET otp_code = $1 WHERE id = $2 RETURNING email",
        new_otp, user_id
    );
    tx.commit();

    if (!result.empty()) {
        auto user_email = result[0][0].as<std::string>();

        if (send_otp(user_email, new_otp)) return true;

        fmt::println(stderr, "Sending OTP failed");
        return false;
    }

    fmt::println("Updating OTP is failed.");
    return false;
}