// NetworkMonitor.cpp — IcmpSendEcho ping + cpr HTTP 检测实现
#include "NetworkMonitor.h"

#include <cpr/cpr.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include "Logger.h"

#include <chrono>
#include <thread>

namespace usc {

bool NetworkMonitor::checkByPing(const std::string& target, int count, double lossThresholdPercent) {
    // --- 域名解析（IcmpSendEcho 需要 IP） ---
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw PingException("WSAStartup 失败");
    }

    struct sockaddr_in dest {};
    int resolveResult = inet_pton(AF_INET, target.c_str(), &dest.sin_addr);
    if (resolveResult != 1) {
        // 不是合法 IP 字符串，尝试域名解析
        addrinfo hints{};
        hints.ai_family = AF_INET;
        addrinfo* res = nullptr;
        if (getaddrinfo(target.c_str(), nullptr, &hints, &res) != 0 || res == nullptr) {
            WSACleanup();
            throw PingException("无法解析目标地址: " + target);
        }
        dest = *reinterpret_cast<sockaddr_in*>(res->ai_addr);
        freeaddrinfo(res);
    }

    // --- IcmpSendEcho 逐个发包统计 ---
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        WSACleanup();
        throw PingException("IcmpCreateFile 失败");
    }

    const WORD payloadSize = 32;
    unsigned char sendData[payloadSize] = "usc-auto-ping-probe";  // 任意内容
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + payloadSize + 8;
    auto* reply = static_cast<ICMP_ECHO_REPLY*>(malloc(replySize));
    if (reply == nullptr) {
        IcmpCloseHandle(hIcmp);
        WSACleanup();
        throw PingException("内存分配失败");
    }

    int received = 0;
    for (int i = 0; i < count; ++i) {
        DWORD rc = IcmpSendEcho(hIcmp, dest.sin_addr.S_un.S_addr,
                                sendData, payloadSize, nullptr,
                                reply, replySize, 3000);  // 单包超时 3s，对齐原版整体超时量级
        if (rc > 0 && reply->Status == IP_SUCCESS) {
            ++received;
        }
        if (i + 1 < count) std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    free(reply);
    IcmpCloseHandle(hIcmp);
    WSACleanup();

    const double lossRate = (count - received) * 100.0 / count;
    if (lossRate > 0.1) {
        log("ping " + target + ": 丢包率 " + std::to_string(lossRate).substr(0, 4) + "%");
    }
    return lossRate <= lossThresholdPercent;
}

bool NetworkMonitor::checkByHttp(const std::string& url, int timeoutSeconds) {
    cpr::Response r = cpr::Head(
        cpr::Url{url},
        cpr::Timeout{static_cast<int>(timeoutSeconds * 1000)},
        cpr::Redirect{false}  // 对齐 Python allow_redirects=False
    );

    // cpr 的 error 字段对应传输层失败（超时/连接拒绝/DNS）
    if (r.error) {
        throw NetworkUnreachableException("HTTP 检测请求失败: " + r.error.message);
    }

    // 对齐原版白名单
    switch (r.status_code) {
        case 200: case 204: case 400: case 401: case 403: case 404:
            return true;
        default:
            log("HTTP 检测异常状态码: " + std::to_string(r.status_code));
            return false;
    }
}

bool NetworkMonitor::check(const AppConfig& cfg) {
    switch (cfg.checkType) {
        case CheckType::Ping:
            return checkByPing(cfg.checkParams.pingTarget, cfg.checkParams.pingCount,
                               cfg.checkParams.pingLossThreshold);
        case CheckType::Http:
            return checkByHttp(cfg.checkParams.httpUrl, cfg.checkParams.httpTimeout);
        case CheckType::Status:
            // 原版逻辑：两种方式都未启用时直接 check_status(auth_server)，由 CoreWorker 分发
            throw ErrorException("Status 类型应由 AuthClient::checkStatus 处理");
    }
    throw ErrorException("未知检测类型");
}

}  // namespace usc
