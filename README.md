# USC-Auto (C++)

南华大学校园网自动认证守护程序 —— C++20 / Qt6 实现（由 Python 版重构而来）。

## 功能

- 网络连通性自动检测（HTTP 探测 / ICMP ping 两种方式，可配置）
- 校园网认证状态监测，掉线自动重新认证（登出 → 重登）
- 认证凭证（Cookie/CSRF Token）自动获取，超 7 天自动刷新并回写配置
- WLAN 连接检测（原生 Windows WLAN API，不受系统语言影响）
- 系统托盘常驻 + 主窗口（实时状态主页：状态/倒计时/账号信息/状态历史；日志子页面：自动刷新 / 保存 / 清空）

## 架构（三层）

```
启动：配置严格校验（不通过 → 弹窗报错退出）
  │
  ├─ CoreWorker   唯一后台线程（std::jthread）：全部业务逻辑 + 状态机埋点
  │                 检测 → 认证 → 重试时序，循环内自捕获异常自愈
  │                 （stateChanged / accountStatusUpdated 信号供 GUI 订阅）
  │
  └─ GUI          纯渲染层（主线程）：MainWindow（状态主页 + 日志子页）+ TrayController 托盘
                    只订阅信号 / 转发用户意图，不含业务
```

## 源码结构

```
src/
├── main.cpp           入口：校验前置 → SSID 校验 → CoreWorker → 主窗口+托盘接线
├── CoreWorker.h/cpp   后台工作线程（60s 检测循环 / reauth 防重入 / 可中断睡眠 / 状态机埋点）
├── AuthClient.h/cpp   认证 API（凭证获取/状态/登录/登出/重认证 + AccountStatus 完整查询）
├── NetworkMonitor.h/cpp  HTTP/IcmpSendEcho 连通性检测
├── WlanChecker.h/cpp  原生 WLAN API SSID 检测
├── AppConfig.h/cpp    config.json 加载/严格校验/回写
├── Logger.h/cpp       环形日志缓冲（10MB）
├── MainWindow.h/cpp   主窗口（QTabWidget：「状态」主页 +「日志」子页）
├── StatusPage.h/cpp   状态主页（状态大图标/倒计时/账号信息/状态历史/控制按钮）
├── LogWindow.h/cpp    日志子页面（自动刷新 / 保存 / 清空）
├── TrayController.h/cpp 系统托盘
└── exceptions.h       异常层次
```

## 构建（Windows / MSYS2 MinGW-w64）

依赖（UCRT64 环境 pacman 安装）：

```bash
pacman -S mingw-w64-ucrt-x86_64-{cmake,ninja,qt6-base,cpr,nlohmann-json}
```

编译：

```bash
cd usc-auto-cpp
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 使用

1. 将 `config.json` 放到 exe 同目录（首次运行会生成模板）
2. 填写认证服务器、账号密码；检测方式按需启用 ping/HTTP
3. 运行 exe → 托盘常驻，双击图标查看日志

## 配置说明（config.json）

| 字段 | 说明 |
|---|---|
| `target_ssid` | 目标 WLAN 名，空 = 不校验 |
| `auth_server` | 认证服务器（自动去 URL 前缀取主机名） |
| `UserCredentials` | 账号 / 密码 |
| `check_network_stability` | with_ping / with_http 二选一启用，都不启用则直查认证状态 |
| `key` | cookie / csrf_token / LastUpdate（自动维护，无需手填） |

## 分支说明

- `c++`：当前实现（默认分支）
- `Python`：Python 版（封存）
- `java`：最初的 Java 实现（封存）
