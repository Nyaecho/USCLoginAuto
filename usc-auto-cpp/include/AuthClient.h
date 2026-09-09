// AuthClient.h — 校园网认证 API 客户端（对齐 core_module.py 的 4 个 API + reAuth）
#pragma once

#include <string>
#include <utility>

#include "exceptions.h"

namespace usc {

// 登录结果（对齐 Python login() 返回值语义）
enum class LoginResult {
    Success = 0,            // 认证成功
    WrongCredentials = 1,   // 账号或密码错误
    CookieOrTokenInvalid = 2, // CSRF token / cookie 无效（HTTP 400）
    OtherError = -1,        // 其他错误
};

// 获取到的凭证
struct Credentials {
    std::string yudearCookie;
    std::string csrfToken;
};

// 账号在线状态详情（GET /api/account/status 完整解析，供 GUI 展示）
// valid=false 表示请求/解析失败（error 给出原因）；不拖异常，GUI 可直接消费
struct AccountStatus {
    bool valid = false;        // 本次查询是否成功拿到有效响应
    bool online = false;       // code==0 且 msg=="在线"
    bool timedOut = false;     // 请求超时（对齐 checkStatus 语义：按"未在线"处理）
    std::string error;         // valid=false 时的原因
    std::string name;          // 姓名（如"新校区学生"）
    std::string username;      // 登录账号
    std::string ipv4;          // 分配的 IPv4
    std::string ipv6;          // 分配的 IPv6（可为空）
    std::string mac;           // 终端 MAC
    std::string onlineSince;   // 上线时间（原始字符串，如 "2026-09-09T14:13:56+08:00"）
    std::string sessionId;     // 会话 ID
    long long bytesUp = 0;     // 上行流量（字节）＝服务器视角 BytesIn4+BytesIn6（它收到的）
    long long bytesDown = 0;   // 下行流量（字节）＝服务器视角 BytesOut4+BytesOut6（它发出的）
};

class AuthClient {
public:
    // Python: get_cookie_and_csrf
    // GET /api/csrf-token，从响应 JSON 取 csrf_token、Set-Cookie 头取 yudear
    // 抛 GetTokenAndCookieException（业务失败）/ NetworkUnreachableException（传输失败）
    static Credentials getCookieAndCsrf(const std::string& authServer);

    // Python: check_status
    // GET /api/account/status；200+code0+msg在线 → true；200+code1+不在线 → false
    // 超时按"未连接"返回 false（对齐原版）；其他异常抛 GetStatusException
    static bool checkStatus(const std::string& authServer);

    // 完整查询账号状态（同一次 GET /api/account/status，解析 online 对象全字段）
    // 任何失败都不抛：置 valid=false + error 原因，供 GUI 安全消费
    // 超时视为"未在线"（对齐 checkStatus 语义）
    static AccountStatus queryStatus(const std::string& authServer);

    // Python: logout
    // GET /api/account/logout；code 0/1 均视为成功返回 true
    // 抛 LogoutException（网络错误/业务失败）
    static bool logout(const std::string& authServer);

    // Python: login
    // POST /api/account/login，表单编码，携带 yudear cookie + X-CSRF-Token 头
    // 超时抛 LoginException；传输错误抛 LoginException（对齐原版包装行为）
    static LoginResult login(const std::string& authServer, const std::string& cookieValue,
                             const std::string& csrfToken, const std::string& username,
                             const std::string& password, int nasId = 1,
                             const std::string& isp = "local");

    // Python: re_auth（logout → sleep 2s → login → sleep 10s）
    // 供 GUI"重新认证"按钮与核心循环复用；sleepSec 参数便于测试注入
    static void reAuth(const std::string& authServer, const std::string& cookieValue,
                       const std::string& csrfToken, const std::string& username,
                       const std::string& password);
};

}  // namespace usc
