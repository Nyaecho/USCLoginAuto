// WlanChecker.cpp — 原生 WLAN API 实现（wlanapi.h）
#include "WlanChecker.h"

#include <windows.h>
#include <wlanapi.h>

#include <algorithm>

namespace usc {

std::optional<WlanState> WlanChecker::query() {
    // 与 wlanapi 链接约定保持一致（不加 WLAN_API_SUFFIX 宏时用 WlanOpenHandle）
    DWORD negotiatedVersion = 0;
    HANDLE hClient = nullptr;
    // 2.0 客户端版本即可满足 EnumInterfaces/QueryInterface
    if (WlanOpenHandle(2, nullptr, &negotiatedVersion, &hClient) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    PWLAN_INTERFACE_INFO_LIST ifList = nullptr;
    WlanState state;
    bool gotAny = false;

    if (WlanEnumInterfaces(hClient, nullptr, &ifList) == ERROR_SUCCESS && ifList != nullptr) {
        for (DWORD i = 0; i < ifList->dwNumberOfItems; ++i) {
            const WLAN_INTERFACE_INFO& info = ifList->InterfaceInfo[i];
            if (info.isState == wlan_interface_state_connected) {
                state.connected = true;
                // 当前关联 SSID 需经 WlanQueryInterface(wlan_intf_opcode_current_connection) 获取
                PVOID pData = nullptr;
                DWORD dataSize = 0;
                // 签名: WlanQueryInterface(hClient, guid, opcode, reserved, &dataSize, &pData, &type)
                if (WlanQueryInterface(hClient, &info.InterfaceGuid,
                                       wlan_intf_opcode_current_connection, nullptr,
                                       &dataSize, &pData, nullptr) == ERROR_SUCCESS &&
                    pData != nullptr) {
                    const auto* attrs = static_cast<const WLAN_CONNECTION_ATTRIBUTES*>(pData);
                    const DOT11_SSID& ssid = attrs->wlanAssociationAttributes.dot11Ssid;
                    if (ssid.uSSIDLength > 0 && ssid.uSSIDLength <= sizeof(ssid.ucSSID)) {
                        state.ssid.assign(reinterpret_cast<const char*>(ssid.ucSSID), ssid.uSSIDLength);
                    }
                    WlanFreeMemory(pData);
                }
                gotAny = true;
                break;  // 取第一个已连接接口即可（对齐原版 netsh 解析行为）
            }
        }
        if (!gotAny && ifList->dwNumberOfItems > 0) {
            gotAny = true;  // 有接口但都未连接
        }
        WlanFreeMemory(ifList);
    }

    WlanCloseHandle(hClient, nullptr);
    if (!gotAny) return std::nullopt;  // 无无线接口/服务不可用
    return state;
}

bool WlanChecker::isConnected(const std::string& expectedSsid) {
    auto state = query();
    if (!state || !state->connected) {
        return false;
    }
    if (expectedSsid.empty()) {
        return true;  // 未指定目标 SSID，只要连着 WiFi 就通过（对齐原版）
    }
    return state->ssid == expectedSsid;
}

}  // namespace usc
