#pragma once

#include <stdexcept>
#include <string>

namespace ppa::api {

class ApiError : public std::runtime_error {
public:
    ApiError(int status, std::string code, std::string message)
        : std::runtime_error(message), _status(status), _code(std::move(code)) {}

    [[nodiscard]] int status() const { return _status; }
    [[nodiscard]] const std::string& code() const { return _code; }

private:
    int _status;
    std::string _code;
};

}  // namespace ppa::api
