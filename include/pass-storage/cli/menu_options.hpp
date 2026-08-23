#pragma once

#include <string_view>

namespace cli {
    enum class Menu_Option {
        Exit = 0,
        Get = 1,
        Delete = 3,
        Append = 4,
        Settings = 5,
        Unknown
    };

    Menu_Option parse_option(std::string_view input) noexcept;
}