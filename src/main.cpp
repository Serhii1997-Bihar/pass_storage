#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include "pass-storage/database/db.hpp"
#include "pass-storage/services/auth.hpp"
#include "pass-storage/services/data.hpp"
#include "pass-storage/services/registration.hpp"
#include "pass-storage/helpers/text.hpp"
#include "pass-storage/services/otp.hpp"
#include "pass-storage/user/user.hpp"
#include "pass-storage/services/saver.hpp"

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

        bool is_running = true;
        while (is_running) {
            fmt::println("Do you have an account ??? (Yes or No)");
            std::string has_account;
            std::getline(std::cin >> std::ws, has_account);

            std::optional<int> user_id;
            bool is_auth = false;
            std::string email;

            if (convert_lower(has_account) == "yes") {
                std::tie(user_id, email) = auth.basic_authentication();
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

            if (user_id.has_value() && is_auth) {
                const std::string username = user_manager.get_username(user_id.value());
                fmt::println("Welcome, {}! What action do you want to do?", username);
                fmt::println("1. Get; 2. Update; 3. Delete; 4. Append; 5. Settings");

                std::string response;
                std::getline(std::cin >> std::ws, response);

                if (response == "5") {
                    fmt::println("What do you want to change? (email/password)");
                    std::string object;
                    std::getline(std::cin >> std::ws, object);

                    fmt::println("Put your new data");
                    std::string new_data;
                    std::getline(std::cin >> std::ws, new_data);

                    const User_Manager::Verify_Object target = (object == "email")
                        ? User_Manager::Verify_Object::Email
                        : User_Manager::Verify_Object::Password;

                    user_manager.update_data(*user_id, target, new_data);

                } else {
                    fmt::println("Put full path to target value");
                    std::string path_clean;
                    std::getline(std::cin >> std::ws, path_clean);

                    const Data_Manager::Data_Path path = data_manager.adapt_path(path_clean);

                    if (response == "4") {
                        data_manager.append_folder(*user_id, path);

                        fmt::println("Put your value (text or path to file)");
                        std::string value;
                        std::getline(std::cin >> std::ws, value);

                        if (path.type_folder == "text") {
                            data_manager.append_data(*user_id, Data_Manager::Data_Path{
                                .type_folder = path.type_folder,
                                .name_folder = path.name_folder,
                                .key = path.key,
                                .value = value
                            });

                        } else if (path.type_folder == "files") {
                            data_manager.append_file(*user_id, Data_Manager::Data_Path{
                                .type_folder = path.type_folder,
                                .name_folder = path.name_folder,
                                .key = path.key
                            }, value);
                        }

                    } else if (response == "1") {
                        const std::string result = data_manager.get_data(*user_id, path);
                        const std::string password = save_manager.save_data(*user_id, result, path);
                        fmt::println("{}", password);

                    } else if (response == "3") {
                        data_manager.delete_data(*user_id, path);

                    } else if (response == "2") {
                        fmt::println("Put your new data");
                        std::string data;
                        std::getline(std::cin >> std::ws, data);

                        data_manager.update_data(*user_id, path, data);
                    }
                }
            }

            is_running = false;
        }

    } catch (const std::exception& e) {
        fmt::println(stderr, "Critical error: {}", e.what());
        return 1;
    }

    return 0;
}