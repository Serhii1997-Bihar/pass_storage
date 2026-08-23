#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <span>
#include <fmt/ostream.h>
#include <botan/secmem.h>
#include "pass-storage/database/db.hpp"
#include "pass-storage/services/auth.hpp"
#include "pass-storage/services/data.hpp"
#include "pass-storage/services/registration.hpp"
#include "pass-storage/helpers/text.hpp"
#include "pass-storage/services/otp.hpp"
#include "pass-storage/user/user.hpp"
#include "pass-storage/services/saver.hpp"
#include "pass-storage/cli/menu_options.hpp"

int main() {
    fmt::println("Hello, it's your own password manager - PassStorage!");

    try {
        DB_Manager db_manager;
        Auth auth(db_manager.get_connection());
        Registration registration(db_manager.get_connection());
        Data_Manager data_manager(db_manager.get_connection());
        User_Manager user_manager(db_manager.get_connection());
        Otp_Manager otp_manager(db_manager.get_connection());
        Save_Manager save_manager(db_manager.get_connection());

        fmt::println("Do you have an account ??? (Yes or No)");
        std::string has_account;
        std::getline(std::cin >> std::ws, has_account);

        std::optional<int> user_id;
        Botan::secure_vector<unsigned char> master_key;

        bool is_auth = false;
        while (!is_auth) {
            if (convert_lower(has_account) == "yes") {
                auto auth_result = auth.basic_authentication();

                user_id = std::get<0>(auth_result);
                std::string user_email = std::get<1>(auth_result);
                master_key = std::get<2>(auth_result);

                if (user_id.has_value()) {
                    if (!otp_manager.update_otp(user_id.value())) {
                        fmt::println(stderr, "Critical error updating OTP!");
                    }

                    is_auth = auth.advanced_authentication(user_id.value());
                }

            } else {
                user_id = registration.registration();
                if (user_id.has_value()) {
                    is_auth = true;
                }
            }
        }

        const std::string username = user_manager.get_username(user_id.value());
        fmt::println("Welcome, {}! What action do you want to do?", username);

        if (user_id.has_value() && is_auth) {
            bool is_running = true;

            while (is_running) {
                fmt::println("1. Get; 3. Delete; 4. Append; 5. Settings; 0. Exit");

                std::string response;
                std::getline(std::cin, response);

                std::span<const unsigned char> mk_span(master_key.data(), master_key.size());

                switch (const auto option = cli::parse_option(response)) {
                    case cli::Menu_Option::Settings: {
                        fmt::println("What do you want to change? (email/password)");
                        std::string object;
                        std::getline(std::cin, object);

                        fmt::println("Put your new data");
                        std::string new_data;
                        std::getline(std::cin, new_data);

                        const User_Manager::Verify_Object target = (object == "email")
                            ? User_Manager::Verify_Object::Email
                            : User_Manager::Verify_Object::Password;

                        user_manager.update_data(*user_id, target, new_data);
                        break;
                    }

                    case cli::Menu_Option::Append:
                    case cli::Menu_Option::Get:
                    case cli::Menu_Option::Delete: {
                        fmt::println("Put full path to target value");
                        std::string path_clean;
                        std::getline(std::cin, path_clean);

                        const Data_Manager::Data_Path path = data_manager.adapt_path(path_clean);

                        if (option == cli::Menu_Option::Append) {
                            data_manager.append_folder(*user_id, path);

                            fmt::println("Put your value (text or path to file)");
                            std::string value;
                            std::getline(std::cin, value);

                            if (path.type_folder == "text") {
                                data_manager.append_data(*user_id, Data_Manager::Data_Path{
                                    .type_folder = path.type_folder,
                                    .name_folder = path.name_folder,
                                    .key = path.key,
                                    .value = value
                                }, mk_span);

                            } else if (path.type_folder == "files") {
                                data_manager.append_file(*user_id, Data_Manager::Data_Path{
                                    .type_folder = path.type_folder,
                                    .name_folder = path.name_folder,
                                    .key = path.key
                                }, value, mk_span);
                            }

                        } else if (option == cli::Menu_Option::Get) {
                            const std::string result = data_manager.get_data(*user_id, path, mk_span);
                            const std::string password = save_manager.save_data(*user_id, result, path, mk_span);
                            fmt::println("{}", password);

                        } else if (option == cli::Menu_Option::Delete) {
                            data_manager.delete_data(*user_id, path);
                        }
                        break;
                    }

                    case cli::Menu_Option::Exit:
                        is_running = false;
                        break;

                    case cli::Menu_Option::Unknown:
                    default:
                        fmt::println("Invalid command. Please try again.");
                        break;
                }
            }
        }

    } catch (const std::exception& e) {
        fmt::println(stderr, "Critical error: {}", e.what());
        return 1;
    }

    return 0;
}