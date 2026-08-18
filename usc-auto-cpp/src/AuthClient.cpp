// AuthClient.cpp — 认证 API 实现（cpr，对齐 core_module.py 请求细节）
#include "AuthClient.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "Logger.h"

#include <chrono>
#include <thread>

namespace usc {

using nlohmann::json;

// 统一判定：cpr error 是否传输层失败（对应 requests 的 RequestException）
[[nodiscard]] static bool isNetworkError(const cpr::Response& r) {
    return static_cast<bool>(r.error);
}

Credentials AuthClient::getCookieAndCsrf(const std::string& authServer) {
    cpr::Response r = cpr::Get(
        cpr::Url{"http://" + authServer + "/api/csrf-token"},
        cpr::Header{
            {"X-Requested-With", "XMLHttpRequest"},
            {"Accept-Language", "zh-CN,zh;q=0.9"},
            {"Connection", "keep-alive"},
        },
        cpr::Timeout{10 * 1000});

    if (isNetworkError(r)) {
        throw NetworkUnreachableException("获取 CSRF Token 网络错误: " + r.error.message);
    }
    if (r.status_code != 200) {
        throw GetTokenAndCookieException("获取 CSRF Token 失败: HTTP " + std::to_string(r.status_code));
    }

    // 解析 JSON 取 csrf_token
    json data;
    try {
        data = json::parse(r.text);
    } catch (const json::parse_error&) {
        throw GetTokenAndCookieException("CSRF Token 响应不是有效 JSON");
    }
    if (!data.is_object() || !data.contains("csrf_token") || !data["csrf_token"].is_string() ||
        data["csrf_token"].get<std::string>().empty()) {
        throw GetTokenAndCookieException("响应中缺少 CSRF Token");
    }
    Credentials cred;
    cred.csrfToken = data["csrf_token"].get<std::string>();
    log("成功获取 CSRF Token: " + cred.csrfToken.substr(0, 5) + "...");

    // 从 Set-Cookie 头提取 yudear
    for (const auto& [name, value] : r.header) {
        if (name == "set-cookie" || name == "Set-Cookie") {
            const std::string& cookies = value;
            const auto pos = cookies.find("yudear=");
            if (pos != std::string::npos) {
                auto end = cookies.find(';', pos);
                if (end == std::string::npos) end = cookies.size();
                cred.yudearCookie = cookies.substr(pos + 7, end - pos - 7);
            }
        }
    }
    if (!cred.yudearCookie.empty()) {
        log("Cookie: yudear=" + cred.yudearCookie.substr(0, 5) + "...");
    } else {
        log("Cookie: 未获取到 yudear");
    }
    return cred;
}

bool AuthClient::checkStatus(const std::string& authServer) {
    cpr::Response r = cpr::Get(
        cpr::Url{"http://" + authServer + "/api/account/status"},
        cpr::Timeout{10 * 1000});

    // 对齐原版：超时按"未连接"返回 false，不抛异常
    if (isNetworkError(r)) {
        if (r.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
            log("状态检测请求超时");
            return false;
        }
        throw NetworkUnreachableException("状态检测网络错误: " + r.error.message);
    }
    if (r.status_code != 200) {
        throw GetStatusException("状态检测返回 HTTP " + std::to_string(r.status_code));
    }

    json data;
    try {
        data = json::parse(r.text);
    } catch (const json::parse_error&) {
        throw GetStatusException("无法解析状态响应");
    }
    const int code = data.value("code", -1);
    const std::string msg = data.value("msg", "");
    if (code == 0 && msg == "在线") return true;
    if (code == 1 && msg == "不在线") return false;
    throw GetStatusException("未知的状态响应: code=" + std::to_string(code) + " msg=" + msg);
}

bool AuthClient::logout(const std::string& authServer) {
    cpr::Response r = cpr::Get(
        cpr::Url{"http://" + authServer + "/api/account/logout"},
        cpr::Timeout{10 * 1000});

    if (isNetworkError(r)) {
        throw NetworkUnreachableException("登出网络错误: " + r.error.message);
    }
    if (r.status_code != 200) {
        // 对齐原版：非 200 打日志返回 false（不抛）
        log("登出请求返回 HTTP " + std::to_string(r.status_code));
        return false;
    }

    json data;
    try {
        data = json::parse(r.text);
    } catch (const json::parse_error&) {
        throw ErrorException("无法解析登出响应");
    }
    const int code = data.value("code", -1);
    if (code == 0) {
        log("登出成功");
        return true;
    }
    if (code == 1) {
        log("已离线，无需重复登出");
        return true;
    }
    throw LogoutException("登出失败: " + data.value("msg", "") + " (code=" + std::to_string(code) + ")");
}

LoginResult AuthClient::login(const std::string& authServer, const std::string& cookieValue,
                              const std::string& csrfToken, const std::string& username,
                              const std::string& password, int nasId, const std::string& isp) {
    // === 参数校验（对齐原版：校验失败打印并返回 OtherError，不抛） ===
    if (cookieValue.empty() || csrfToken.empty() || username.empty() || password.empty()) {
        log("Cookie/Token/用户名/密码不能为空");
        return LoginResult::OtherError;
    }

    cpr::Response r = cpr::Post(
        cpr::Url{"http://" + authServer + "/api/account/login"},
        cpr::Cookies{{"yudear", cookieValue}},  // 关键：token 与同会话 cookie 配对校验
        cpr::Header{
            {"X-CSRF-Token", csrfToken},
            {"X-Requested-With", "XMLHttpRequest"},
            {"Accept-Language", "zh-CN,zh;q=0.9"},
            {"Accept", "*/*"},
            {"Content-Type", "application/x-www-form-urlencoded; charset=UTF-8"},
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.6723.70 Safari/537.36"},
            {"Origin", "http://" + authServer},
            {"Referer", "http://" + authServer + "/tpl/default/login_account.html?ip=10.14.75.198&nasId=" + std::to_string(nasId)},
            {"Connection", "keep-alive"},
        },
        // cpr::Payload = application/x-www-form-urlencoded 请求体（Parameters 是 URL 查询串，勿混用）
        cpr::Payload{
            {"username", username},
            {"password", password},
            {"nasId", std::to_string(nasId)},
            {"isp", isp},
            {"timeLimit", ""},
        },
        cpr::Timeout{15 * 1000});

    if (isNetworkError(r)) {
        // 对齐原版：传输层失败统一包成 LoginException 抛出
        throw LoginException("登录网络错误: " + r.error.message);
    }

    if (r.status_code == 400) {
        // CSRF token / cookie 无效
        log("CSRF Token 或 Cookie 无效 (HTTP 400)");
        return LoginResult::CookieOrTokenInvalid;
    }

    if (r.status_code == 200) {
        json result;
        try {
            result = json::parse(r.text);
        } catch (const json::parse_error&) {
            log("登录响应不是有效的 JSON");
            return LoginResult::OtherError;
        }
        const int code = result.value("code", -1);
        const std::string msg = result.value("msg", "");
        if (code == 0 && msg.find("认证成功") != std::string::npos) {
            log("登录成功");
            return LoginResult::Success;
        }
        if (code == 1 && (msg.find("账号或密码错误") != std::string::npos ||
                          std::to_string(result.value("authCode", 0)).find("E20002") != std::string::npos)) {
            log("账号或密码错误");
            return LoginResult::WrongCredentials;
        }
        log("未知登录响应: code=" + std::to_string(code) + " msg=" + msg);
        return LoginResult::OtherError;
    }

    log("登录返回 HTTP " + std::to_string(r.status_code));
    return LoginResult::OtherError;
}

void AuthClient::reAuth(const std::string& authServer, const std::string& cookieValue,
                        const std::string& csrfToken, const std::string& username,
                        const std::string& password) {
    logout(authServer);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    login(authServer, cookieValue, csrfToken, username, password);
    std::this_thread::sleep_for(std::chrono::seconds(10));
}

}  // namespace usc
