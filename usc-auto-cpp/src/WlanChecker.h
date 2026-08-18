// WlanChecker.h — 原生 WLAN API 检测无线连接状态与 SSID
// 替代 Python 版 netsh wlan show interfaces 子进程 + 正则解析（根治中英文系统语言问题）
#pragma once

#include <optional>
#include <string>

namespace usc {

struct WlanState {
    bool connected = false;   // 是否处于已连接状态
    std::string ssid;         // 当前 SSID（未连接时为空）
};

class WlanChecker {
public:
    // 查询当前首个 WLAN 接口状态。失败（如无无线网卡/服务未启动）返回 nullopt。
    static std::optional<WlanState> query();

    // 便捷判断：已连接且（expectedSsid 为空 或 SSID 匹配）
    // 对齐 Python is_connected_wlan：不匹配/未连接返回 false，查询失败按未连接处理
    static bool isConnected(const std::string& expectedSsid);
};

}  // namespace usc
