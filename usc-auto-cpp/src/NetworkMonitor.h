// NetworkMonitor.h — 网络连通性检测（ping / HTTP 两种方式）
#pragma once

#include <string>

#include "AppConfig.h"
#include "exceptions.h"

namespace usc {

class NetworkMonitor {
public:
    // IcmpSendEcho 方式检测（替代 Python 的 ping 子进程 + 正则解析丢包率）
    // 返回 true = 网络稳定（丢包率 <= lossThresholdPercent）
    // 抛 PingException：ping 命令本身失败（如目标不可解析）
    static bool checkByPing(const std::string& target, int count, double lossThresholdPercent);

    // cpr HEAD 请求方式检测（对齐 Python check_network_stability_http）
    // 返回 true = 收到白名单状态码（200/204/400/401/403/404）
    // 抛 HTTPCheckException / NetworkUnreachableException：请求失败
    static bool checkByHttp(const std::string& url, int timeoutSeconds);

    // 统一分发入口（对齐 Python check_network(type, **data)）
    // type==Ping → checkByPing；type==Http → checkByHttp；type==Status → 抛错提示用 AuthClient
    // 抛 NetworkUnreachableException 映射 requests 的连接类异常
    static bool check(const AppConfig& cfg);
};

}  // namespace usc
