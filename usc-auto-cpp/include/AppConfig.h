// AppConfig.h — config.json 加载 + 启动期严格校验（v2 分层：校验不通过即报错退出）
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace usc {

// 校验错误（结构化：字段路径 + 人话原因），启动失败时弹窗逐条展示
struct ConfigError {
    std::string field;   // 如 "UserCredentials.username"
    std::string reason;  // 如 "不能为空"
};

// 网络稳定性检测方式（对齐 Python 版 check_network 的 type 分支）
enum class CheckType {
    Ping,   // with_ping.enabled = true
    Http,   // with_http.enabled = true
    Status, // 两者都未启用：退化为直接查认证服务器状态（对齐原版 else 分支）
};

// 检测参数（tagged union 风格，按 CheckType 取对应字段）
struct CheckParams {
    // ping
    std::string pingTarget = "202.89.233.100";
    int pingCount = 10;
    double pingLossThreshold = 50.0;  // 百分比
    // http
    std::string httpUrl = "http://connectivitycheck.platform.hicloud.com/generate_204";
    int httpTimeout = 10;  // 秒
};

struct AppConfig {
    std::string targetSsid;                 // 可为空 = 不校验 SSID
    std::string authServer;                 // 认证服务器主机名
    std::string username;
    std::string password;
    std::string cookie;                     // yudear cookie
    std::string csrfToken;                  // CSRF token
    std::string lastUpdate;                 // "YYYY-MM-DD HH:MM:SS"
    CheckType checkType = CheckType::Status;
    CheckParams checkParams;

    // 加载并校验。返回错误列表；空列表 = 通过。
    // failFast: 校验到第一个错误就返回（启动弹窗场景），否则收集全部（便于用户一次改完）
    static std::vector<ConfigError> loadAndValidate(const std::string& configPath, AppConfig& out);

    // 生成默认配置模板（对齐 Python main.pyw 的 default_config），写到 configPath
    static bool writeDefault(const std::string& configPath, std::string& errMsg);

    // 把更新后的 cookie/token/lastUpdate 回写进 json（保留其他字段不动）
    // 返回 false + errMsg 表示写文件失败
    static bool saveKeys(const std::string& configPath, const std::string& cookie,
                         const std::string& csrfToken, const std::string& lastUpdate,
                         std::string& errMsg);

    // 解析 "YYYY-MM-DD HH:MM:SS"（本地时间）为 Unix 秒；失败返回 nullopt
    static std::optional<std::int64_t> parseTimestamp(const std::string& ts);

    // Unix 秒 → "YYYY-MM-DD HH:MM:SS"（本地时间），用于回写 LastUpdate
    static std::string formatTimestamp(std::int64_t unixSec);

private:
    // auth_server 归一化：带 http(s):// 前缀时只取主机名（对齐原版 urlparse().hostname）
    static std::string normalizeAuthServer(const std::string& raw);
};

}  // namespace usc
