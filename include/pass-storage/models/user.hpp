#pragma once

#include <string>

struct User {
    std::string email;
    std::string password;
    std::string username;
    std::string question;
    std::string answer;
    std::string otp;
    int phone;
};