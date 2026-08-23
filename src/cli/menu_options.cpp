#include "pass-storage/cli/menu_options.hpp"

namespace cli {
    Menu_Option parse_option(const std::string_view input) noexcept {
        if (input == "0") return Menu_Option::Exit;
        if (input == "1") return Menu_Option::Get;
        if (input == "3") return Menu_Option::Delete;
        if (input == "4") return Menu_Option::Append;
        if (input == "5") return Menu_Option::Settings;
        return Menu_Option::Unknown;
    }
}