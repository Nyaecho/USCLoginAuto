// AppConfig.cpp — config.json 加载、校验、回写实现
#include "AppConfig.h"

#include <nlohmann/json.hpp>

#include <ctime>
#include <fstream>
#include <sstream>

namespace usc {

using nlohmann::json;

// 从 json 字符串安全取值（字段存在且为 string 才返回）
static std::optional<std::string> optString(const json& j, const char* key) {
    if (j.is_object() && j.contains(key) && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return std::nullopt;
}

std::string AppConfig::normalizeAuthServer(const std::string& raw) {
    std::string s = raw;
    const std::string http = "http://";
    const std::string https = "https://";
    std::string host = s;
    if (s.rfind(http, 0) == 0) {
        host = s.substr(http.size());
    } else if (s.rfind(https, 0) == 0) {
        host = s.substr(https.size());
    }
    // 去掉路径 / 端口部分（对齐 urlparse().hostname 只取主机名）
    const auto slash = host.find('/');
    if (slash != std::string::npos) host = host.substr(0, slash);
    const auto colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0, colon);
    return host;
}

std::optional<std::int64_t> AppConfig::parseTimestamp(const std::string& ts) {
    std::tm tm{};
    std::istringstream ss(ts);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) return std::nullopt;
    // 对齐 Python time.mktime：按本地时间解释
    const std::time_t t = std::mktime(&tm);
    if (t == -1) return std::nullopt;
    return static_cast<std::int64_t>(t);
}

std::string AppConfig::formatTimestamp(std::int64_t unixSec) {
    const std::time_t t = static_cast<std::time_t>(unixSec);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

std::vector<ConfigError> AppConfig::loadAndValidate(const std::string& configPath, AppConfig& out) {
    std::vector<ConfigError> errors;

    // --- 1. 文件可读 + JSON 可解析 ---
    std::ifstream fin(configPath);
    if (!fin.is_open()) {
        errors.push_back({"config.json", "无法打开文件（请检查路径与权限）"});
        return errors;
    }
    json root;
    try {
        fin >> root;
    } catch (const json::parse_error& e) {
        errors.push_back({"config.json", std::string("JSON 解析失败: ") + e.what()});
        return errors;
    }
    if (!root.is_object()) {
        errors.push_back({"config.json", "根节点必须是对象 {}"});
        return errors;
    }

    AppConfig cfg;

    // --- 2. 必填字段 ---
    if (auto v = optString(root, "auth_server")) {
        cfg.authServer = normalizeAuthServer(*v);
        if (cfg.authServer.empty()) {
            errors.push_back({"auth_server", "不能为空（若填了 URL，未能从中解析出主机名）"});
        }
    } else {
        errors.push_back({"auth_server", "缺失或不是字符串"});
    }

    // --- 3. 账号密码 ---
    if (root.contains("UserCredentials") && root["UserCredentials"].is_object()) {
        const json& uc = root["UserCredentials"];
        cfg.username = optString(uc, "username").value_or("");
        cfg.password = optString(uc, "password").value_or("");
        if (cfg.username.empty()) errors.push_back({"UserCredentials.username", "不能为空"});
        if (cfg.password.empty()) errors.push_back({"UserCredentials.password", "不能为空"});
    } else {
        errors.push_back({"UserCredentials", "缺失或不是对象"});
    }

    // --- 4. 可选字段 ---
    cfg.targetSsid = optString(root, "target_ssid").value_or("");  // 空 = 跳过 SSID 校验

    if (root.contains("key") && root["key"].is_object()) {
        const json& key = root["key"];
        cfg.cookie = optString(key, "cookie").value_or("");
        cfg.csrfToken = optString(key, "csrf_token").value_or("");
        cfg.lastUpdate = optString(key, "LastUpdate").value_or("");
        // cookie/token 允许为空：启动后由 AuthClient 自动获取回写（对齐原版逻辑）
    } else {
        cfg.cookie.clear();
        cfg.csrfToken.clear();
        cfg.lastUpdate.clear();
    }

    // --- 5. 网络检测方式 ---
    const bool pingEnabled =
        root.contains("check_network_stability") &&
        root["check_network_stability"].is_object() &&
        root["check_network_stability"].contains("with_ping") &&
        root["check_network_stability"]["with_ping"].is_object() &&
        root["check_network_stability"]["with_ping"].value("enabled", false);

    const json* httpNode = nullptr;
    if (root.contains("check_network_stability") &&
        root["check_network_stability"].is_object() &&
        root["check_network_stability"].contains("with_http") &&
        root["check_network_stability"]["with_http"].is_object()) {
        httpNode = &root["check_network_stability"]["with_http"];
    }

    if (pingEnabled) {
        const json& p = root["check_network_stability"]["with_ping"];
        cfg.checkType = CheckType::Ping;
        cfg.checkParams.pingTarget = optString(p, "target").value_or("202.89.233.100");
        cfg.checkParams.pingCount = p.value("count", 10);
        cfg.checkParams.pingLossThreshold = p.value("loss_threshold", 50.0);
        if (cfg.checkParams.pingCount < 1) {
            errors.push_back({"check_network_stability.with_ping.count", "至少为 1"});
        }
    } else if (httpNode && httpNode->value("enabled", false)) {
        cfg.checkType = CheckType::Http;
        // 兼容原版两个键名：main.pyw 写 target_url，core_module 读 url（优先 target_url）
        std::string url = optString(*httpNode, "target_url").value_or("");
        if (url.empty()) url = optString(*httpNode, "url").value_or("");
        if (url.empty()) {
            errors.push_back({"check_network_stability.with_http.target_url", "启用 HTTP 检测时不能为空"});
        }
        cfg.checkParams.httpUrl = url;
        cfg.checkParams.httpTimeout = httpNode->value("timeout", 10);
        if (cfg.checkParams.httpTimeout < 1) {
            errors.push_back({"check_network_stability.with_http.timeout", "至少为 1 秒"});
        }
    } else {
        // 两者都未启用：退化为认证服务器状态检测（对齐原版 else 分支）
        cfg.checkType = CheckType::Status;
    }

    if (errors.empty()) {
        out = cfg;  // 全部通过才提交
    }
    return errors;
}

bool AppConfig::writeDefault(const std::string& configPath, std::string& errMsg) {
    json def = {
        {"target_ssid", "Your_SSID_Here"},
        {"auth_server", "Your_Auth_Server_Here"},
        {"UserCredentials", {{"username", "your_username_here"}, {"password", "your_password_here"}}},
        {"check_network_stability",
         {{"with_ping",
           {{"enabled", false},
            {"tips", "这里存放检测网络连通性需要的参数，依次为 ping目标，ping次数，可以接受的丢包率百分比"},
            {"target", "202.89.233.100"},
            {"count", 10},
            {"loss_threshold", 50.0}}},
          {"with_http",
           {{"enabled", true},
            {"tips", "这里存放检测网络连通性需要的参数，依次为 目标URL，超时时间（秒）"},
            {"target_url", "http://connectivitycheck.platform.hicloud.com/generate_204"},
            {"timeout", 5}}}}},
        {"key", {{"cookie", "这里会自动获取"}, {"csrf_token", "这里会自动获取"}, {"LastUpdate", "1999-01-01 00:00:00"}}},
    };

    std::ofstream fout(configPath);
    if (!fout.is_open()) {
        errMsg = "无法写入文件: " + configPath;
        return false;
    }
    fout << def.dump(2) << std::endl;  // 缩进 2，对齐原版 ensure_ascii=False + indent=2
    return fout.good();
}

bool AppConfig::saveKeys(const std::string& configPath, const std::string& cookie,
                         const std::string& csrfToken, const std::string& lastUpdate,
                         std::string& errMsg) {
    // 读-改-写：保留用户其他字段
    std::ifstream fin(configPath);
    if (!fin.is_open()) {
        errMsg = "无法读取配置文件";
        return false;
    }
    json root;
    try {
        fin >> root;
    } catch (const json::parse_error& e) {
        errMsg = std::string("配置文件损坏: ") + e.what();
        return false;
    }
    if (!root.contains("key") || !root["key"].is_object()) {
        root["key"] = json::object();
    }
    root["key"]["cookie"] = cookie;
    root["key"]["csrf_token"] = csrfToken;
    root["key"]["LastUpdate"] = lastUpdate;

    std::ofstream fout(configPath);
    if (!fout.is_open()) {
        errMsg = "无法写入配置文件";
        return false;
    }
    fout << root.dump(2) << std::endl;
    return fout.good();
}

}  // namespace usc
