#include "pass-storage/services/saver.hpp"
#include "pass-storage/crypto/encryption.hpp"
#include "config.hpp"
#include "pass-storage/helpers/uuid.hpp"
#include "pass-storage/helpers/integer.hpp"
#include <nlohmann/json.hpp>
#include <botan/base64.h>
#include <filesystem>
#include <fstream>
#include <format>
#include <cstdlib>
#include <chrono>
#include <fmt/core.h>

Save_Manager::Save_Manager(std::shared_ptr<pqxx::connection> db_connection)
    : db_(std::move(db_connection)) {}

std::string Save_Manager::save_data(
    int user_id,
    const std::string& data,
    const Data_Manager::Data_Path& path,
    std::span<const unsigned char> master_key) const
{
    if (data.empty()) return "No Data";

    namespace fs = std::filesystem;

    const fs::path result_dir = fs::current_path().parent_path() / "result";
    std::error_code error_code;
    fs::create_directories(result_dir, error_code);

    const auto now = std::chrono::system_clock::now();
    const fs::path zip_file = result_dir / std::format("data ({:%Y-%m-%d %H-%M}).7z", std::chrono::zoned_time{std::chrono::current_zone(), now});
    const std::string password = generate_password();

    const fs::path temp_export_dir = result_dir / std::format("temp_{}", generate_uuid_v4());
    fs::create_directories(temp_export_dir, error_code);

    if (path.type_folder == "files") {
        try {
            if (!path.key.empty()) {
                std::string clean_data = data;
                if (clean_data.size() >= 2 && clean_data.front() == '"' && clean_data.back() == '"') {
                    clean_data = clean_data.substr(1, clean_data.size() - 2);
                }

                const std::string file_id = clean_data.substr(0, 36);
                const std::string ext = clean_data.size() > 36 ? clean_data.substr(36) : "";

                const fs::path target_path = temp_export_dir / (path.key + ext);
                const std::string context = std::format("{}/{}/{}", path.type_folder, path.name_folder, path.key);

                get_file(user_id, file_id, target_path, master_key, context);

            } else {
                const auto json_data = nlohmann::json::parse(data, nullptr, false);
                if (!json_data.is_discarded() && json_data.is_object()) {
                    for (const auto& [file_key, file_id_val] : json_data.items()) {
                        if (file_id_val.is_string()) {
                            const std::string raw_val = file_id_val.get<std::string>();
                            const std::string file_id = raw_val.substr(0, 36);
                            const std::string ext = raw_val.size() > 36 ? raw_val.substr(36) : "";

                            const fs::path target_path = temp_export_dir / (file_key + ext);
                            const std::string context = std::format("{}/{}/{}", path.type_folder, path.name_folder, file_key);

                            get_file(user_id, file_id, target_path, master_key, context);
                        }
                    }
                } else {
                    fs::remove_all(temp_export_dir, error_code);
                    return "JSON parsing failed.";
                }
            }
        } catch (const std::exception&) {
            fs::remove_all(temp_export_dir, error_code);
            return "JSON parsing failed.";
        }

    } else {
        const fs::path clean_filename = path.key.empty() ? fs::path(path.name_folder) : fs::path(path.key).filename();
        const fs::path txt_file = temp_export_dir / std::format("{}.txt", clean_filename.string());

        std::ofstream out_stream(txt_file, std::ios::binary);
        if (!out_stream.is_open()) {
            fs::remove_all(temp_export_dir, error_code);
            return "File wasn't opened.";
        }

        if (!path.key.empty()) {
            const std::string context = std::format("{}/{}/{}", path.type_folder, path.name_folder, path.key);
            const auto decoded = Botan::base64_decode(data);
            const std::span<const unsigned char> enc_span(decoded.data(), decoded.size());
            const auto decrypted = Encryption::decrypt(enc_span, master_key, context);

            out_stream.write(reinterpret_cast<const char*>(decrypted.data()), decrypted.size());
        } else {
            const auto json_data = nlohmann::json::parse(data, nullptr, false);
            if (!json_data.is_discarded() && json_data.is_object()) {
                for (const auto& [entry_key, enc_val_json] : json_data.items()) {
                    if (enc_val_json.is_string()) {
                        const std::string b64_str = enc_val_json.get<std::string>();
                        const std::string context = std::format("{}/{}/{}", path.type_folder, path.name_folder, entry_key);

                        const auto decoded = Botan::base64_decode(b64_str);
                        const std::span<const unsigned char> enc_span(decoded.data(), decoded.size());
                        const auto decrypted = Encryption::decrypt(enc_span, master_key, context);

                        out_stream << entry_key << ": "
                                   << std::string_view(reinterpret_cast<const char*>(decrypted.data()), decrypted.size())
                                   << "\n";
                    }
                }
            }
        }

        out_stream.close();
    }

    const std::string command = std::format("\"\"{}\" a -t7z -mhe=on -p\"{}\" \"{}\" \"{}/*\" > nul\"",
                                            config::SEVEN_ZIP_PATH,
                                            password,
                                            zip_file.string(),
                                            temp_export_dir.string());

    const int result = std::system(command.c_str());
    fs::remove_all(temp_export_dir, error_code);
    if (result == 0) return password;

    return "Archiving failed.";
}

bool Save_Manager::get_file(
    int user_id, const std::string& file_id,
    const std::filesystem::path& destination_path,
    std::span<const unsigned char> master_key,
    const std::string& path_context) const
{
    try {
        pqxx::read_transaction tx(*db_);

        const auto res = tx.exec(
            "SELECT content FROM file_storage WHERE file_id = $1::uuid AND user_id = $2",
            pqxx::params{file_id, user_id}
        );

        if (res.empty()) return false;

        const auto field = res[0][0].as<pqxx::bytes>();

        std::span<const unsigned char> enc_span(reinterpret_cast<const unsigned char*>(field.data()), field.size());
        auto decrypted_data = Encryption::decrypt(enc_span, master_key, path_context);

        std::filesystem::create_directories(destination_path.parent_path());
        std::ofstream out_stream(destination_path, std::ios::binary);
        if (!out_stream.is_open()) return false;

        out_stream.write(reinterpret_cast<const char*>(decrypted_data.data()), decrypted_data.size());
        return true;
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error in get_file: {}", e.what());
        return false;
    }
}