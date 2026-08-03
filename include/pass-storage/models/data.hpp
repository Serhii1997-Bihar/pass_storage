#pragma once

#include <string>
#include <nlohmann/json.hpp>

struct Data {
    int id;
    int user_id;
    nlohmann::json data;

    bool is_valid() const {
        return data.is_object() &&
               data.contains("text") &&
               data.contains("files") &&
               data.contains("documents");
    }
};