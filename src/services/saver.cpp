#include "pass-storage/services/saver.hpp"
#include "config.hpp"
#include "pass-storage/helpers/uuid.hpp"
#include "pass-storage/helpers/integer.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <format>
#include <cstdlib>
#include <chrono>
#include <fmt/core.h>

Save_Manager::Save_Manager(std::shared_ptr<pqxx::connection> db_connection)
    : db_(std::move(db_connection)) {}

std::string Save_Manager::save_data(int user_id, const std::string& data, const Data_Manager::Data_Path& path) {
    if (data.empty()) return "No Data";

    namespace fs = std::filesystem;

    const fs::path result_dir = fs::current_path() / "result";
    std::error_code ec;
    fs::create_directories(result_dir, ec);

    const auto now = std::chrono::system_clock::now();
    const fs::path zip_file = result_dir / std::format("data ({:%Y-%m-%d %H-%M}).7z", std::chrono::zoned_time{std::chrono::current_zone(), now});
    const std::string password = generate_password();

    const fs::path temp_export_dir = result_dir / std::format("temp_{}", generate_uuid_v4());
    fs::create_directories(temp_export_dir, ec);

    if (path.type_folder == "files") {
        try {
            const auto json_data = nlohmann::json::parse(data);

            if (json_data.is_object()) {
                for (const auto& [file_name, file_id_val] : json_data.items()) {
                    if (file_id_val.is_string()) {
                        const std::string file_id = file_id_val.get<std::string>();
                        const fs::path target_path = temp_export_dir / file_name;

                        get_file(user_id, file_id, target_path);
                    }
                }
            }
        } catch (const std::exception&) {
            fs::remove_all(temp_export_dir, ec);
            return "JSON parsing failed.";
        }
    } else {
        const fs::path clean_filename = fs::path(path.key).filename();
        const fs::path txt_file = temp_export_dir / std::format("{}.txt", clean_filename.string());

        std::ofstream out_stream(txt_file, std::ios::binary);
        if (!out_stream.is_open()) {
            fs::remove_all(temp_export_dir, ec);
            return "File wasn't opened.";
        }

        out_stream << data;
        out_stream.close();
    }

    const std::string command = std::format("\"\"{}\" a -t7z -mhe=on -p\"{}\" \"{}\" \"{}/*\" > nul\"",
                                            config::SEVEN_ZIP_PATH,
                                            password,
                                            zip_file.string(),
                                            temp_export_dir.string());

    const int result = std::system(command.c_str());

    fs::remove_all(temp_export_dir, ec);

    if (result == 0) return password;

    return "Archiving failed.";
}

bool Save_Manager::get_file(int user_id, const std::string& file_id, const std::filesystem::path& destination_path) {
    try {
        pqxx::read_transaction tx(*db_);

        const auto res = tx.exec(
            "SELECT content FROM file_storage WHERE file_id = $1::uuid AND user_id = $2",
            pqxx::params{file_id, user_id}
        );

        if (res.empty()) return false;

        const auto field = res[0][0].as<pqxx::bytes>();

        std::filesystem::create_directories(destination_path.parent_path());
        std::ofstream out_stream(destination_path, std::ios::binary);
        if (!out_stream.is_open()) return false;

        out_stream.write(reinterpret_cast<const char*>(field.data()), field.size());
        return true;
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in get_file: {}", e.what());
        return false;
    }
}