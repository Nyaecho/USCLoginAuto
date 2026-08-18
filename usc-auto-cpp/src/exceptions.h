// exceptions.h — 对齐 Python 版 UserException.py 的自定义异常层次
#pragma once

#include <stdexcept>
#include <string>

namespace usc {

// Python: PingException
class PingException : public std::runtime_error {
public:
    explicit PingException(const std::string& msg) : std::runtime_error(msg) {}
};

// Python: getTokenAndCookieException
class GetTokenAndCookieException : public std::runtime_error {
public:
    explicit GetTokenAndCookieException(const std::string& msg) : std::runtime_error(msg) {}
};

// Python: getStatusException
class GetStatusException : public std::runtime_error {
public:
    explicit GetStatusException(const std::string& msg) : std::runtime_error(msg) {}
};

// Python: LogoutException
class LogoutException : public std::runtime_error {
public:
    explicit LogoutException(const std::string& msg) : std::runtime_error(msg) {}
};

// Python: LoginException
class LoginException : public std::runtime_error {
public:
    explicit LoginException(const std::string& msg) : std::runtime_error(msg) {}
};

// Python: HTTPCheckException
class HTTPCheckException : public std::runtime_error {
public:
    explicit HTTPCheckException(const std::string& msg) : std::runtime_error(msg) {}
};

// Python: CheckWlanException（用哨兵类型，core 循环按类型分支处理）
class CheckWlanException : public std::runtime_error {
public:
    explicit CheckWlanException(const std::string& msg = "WLAN 未连接") : std::runtime_error(msg) {}
};

// Python: ErrorException（通用）
class ErrorException : public std::runtime_error {
public:
    explicit ErrorException(const std::string& msg) : std::runtime_error(msg) {}
};

// 网络不可达类错误的统一映射（对应 requests.exceptions.RequestException）
class NetworkUnreachableException : public std::runtime_error {
public:
    explicit NetworkUnreachableException(const std::string& msg) : std::runtime_error(msg) {}
};

}  // namespace usc
