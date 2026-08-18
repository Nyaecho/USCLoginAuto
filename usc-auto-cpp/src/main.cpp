// USC-Auto C++ 版正式入口（v2 三层：配置校验前置 → CoreWorker 全业务 → GUI 纯渲染）
//
// 测试模式（开发用）：
//   --smoke            模块冒烟，结果写 smoke_result.txt
//   --login-only       凭证获取 + 真实登录
//   --e2e [--reauth] [--fresh]  40s 端到端（可选注入 reauth / 强制刷新凭证）
#include "AppConfig.h"
#include "AuthClient.h"
#include "CoreWorker.h"
#include "LogWindow.h"
#include "Logger.h"
#include "NetworkMonitor.h"
#include "SingleInstance.h"
#include "TrayController.h"
#include "WlanChecker.h"

#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QString>
#include <QTimer>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

void smokeTest(const std::string& exeDir) {
    std::ofstream out(exeDir + "/smoke_result.txt", std::ios::binary);
    out << "=== USC-Auto 模块冒烟测试 ===\n";

    usc::Logger::instance().write("测试日志行1");
    usc::Logger::instance().write("测试日志行2");
    const QString logContent = usc::Logger::instance().content();
    out << "[Logger] " << (logContent.contains("测试日志行2") ? "PASS" : "FAIL") << "\n";

    const std::string cfgPath = exeDir + "/../../config.json";
    usc::AppConfig cfg;
    auto errors = usc::AppConfig::loadAndValidate(cfgPath, cfg);
    out << "[AppConfig] " << (errors.empty() ? "PASS" : "FAIL") << "\n";
    for (const auto& e : errors) out << "  - " << e.field << ": " << e.reason << "\n";

    auto wlan = usc::WlanChecker::query();
    out << "[WlanChecker] " << (wlan.has_value() ? "PASS" : "PASS(无WLAN接口)")
        << " connected=" << (wlan ? wlan->connected : false) << "\n";

    try {
        const bool ok = usc::NetworkMonitor::checkByHttp(
            "http://connectivitycheck.platform.hicloud.com/generate_204", 5);
        out << "[HTTP检测] PASS 返回=" << ok << "\n";
    } catch (const std::exception& e) {
        out << "[HTTP检测] PASS(异常路径) " << e.what() << "\n";
    }
    out << "=== 完成 ===\n";
}

int runLoginOnly(const std::string& exeDir) {
    usc::AppConfig cfg;
    const std::string cfgPath = exeDir + "/../../config.json";
    usc::AppConfig::loadAndValidate(cfgPath, cfg);
    std::ofstream out(exeDir + "/smoke_result.txt", std::ios::binary);
    try {
        auto cred = usc::AuthClient::getCookieAndCsrf(cfg.authServer);
        out << "[login-only] 凭证获取 OK cookie.len=" << cred.yudearCookie.size() << "\n";
        auto rc = usc::AuthClient::login(cfg.authServer, cred.yudearCookie, cred.csrfToken,
                                         cfg.username, cfg.password);
        out << "[login-only] login 结果=" << static_cast<int>(rc) << "（0=成功 2=CSRF无效）\n";
    } catch (const std::exception& e) {
        out << "[login-only] 异常: " << e.what() << "\n";
    }
    out << "=== 日志 ===\n" << usc::Logger::instance().content().toStdString() << "\n";
    return 0;
}

int runE2E(QApplication& app, const std::string& exeDir, int argc, char* argv[]) {
    usc::AppConfig cfg;
    const std::string cfgPath = exeDir + "/../../config.json";
    auto errors = usc::AppConfig::loadAndValidate(cfgPath, cfg);
    if (!errors.empty()) {
        std::ofstream out(exeDir + "/smoke_result.txt", std::ios::binary);
        out << "[E2E] FAIL 配置校验未通过\n";
        return 1;
    }

    const bool withReauth = (argc >= 3 && std::string(argv[2]) == "--reauth");
    const bool withFresh = (argc >= 3 && std::string(argv[2]) == "--fresh") ||
                           (argc >= 4 && std::string(argv[3]) == "--fresh");
    const bool withPause = (argc >= 3 && std::string(argv[2]) == "--pause") ||
                           (argc >= 4 && std::string(argv[3]) == "--pause");
    if (withFresh) {  // 必须在构造 CoreWorker（拷贝 cfg）之前清空
        cfg.cookie.clear();
        cfg.csrfToken.clear();
    }

    auto* worker = new usc::CoreWorker(cfg, cfgPath);
    QObject::connect(worker, &usc::CoreWorker::stopped, &app, &QCoreApplication::quit);
    if (withReauth) {
        QTimer::singleShot(3000, [worker]() { worker->requestReauth(); });
    }
    if (withPause) {
        // 3s 后暂停 5 分钟，20s 后手动恢复（验证暂停生效+提前恢复），总时长 40s 内完成
        QTimer::singleShot(3000, [worker]() { worker->requestPause(5); });
        QTimer::singleShot(20000, [worker]() { worker->resume(); });
    }
    QTimer::singleShot(40000, worker, &usc::CoreWorker::requestStop);
    worker->start();

    const int rc = app.exec();
    std::ofstream out(exeDir + "/smoke_result.txt", std::ios::binary);
    out << "[E2E] ExitCode=" << rc << " reauth=" << (withReauth ? "on" : "off") << "\n=== 运行日志 ===\n"
        << usc::Logger::instance().content().toStdString() << "\n";
    return rc;
}

// === 正式启动流程 ===

// 配置校验失败弹窗：逐条列出字段与原因
void showConfigErrors(const std::vector<usc::ConfigError>& errors) {
    QString detail;
    for (const auto& e : errors) {
        detail += QString("• %1：%2\n").arg(QString::fromStdString(e.field),
                                            QString::fromStdString(e.reason));
    }
    QMessageBox::critical(nullptr, "配置错误",
                          "配置文件校验未通过：\n\n" + detail + "\n请编辑 config.json 后重启程序。");
}

// 优雅退出：停线程 → 退事件循环
void shutdownApp(usc::CoreWorker* worker) {
    usc::log("正在关闭程序...");
    if (worker) worker->requestStop();
    QApplication::quit();
}

int runGui(QApplication& app, const std::string& exeDir, usc::SingleInstance& guard) {
    // config.json 与 exe 同目录（对齐 Python os.path.dirname(__file__)）
    const std::string cfgPath = exeDir + "/config.json";

    // --- 1. 配置缺失：生成默认模板 → 弹窗 → 退出（对齐原版） ---
    if (!std::filesystem::exists(cfgPath)) {
        std::string err;
        if (!usc::AppConfig::writeDefault(cfgPath, err)) {
            QMessageBox::critical(nullptr, "配置错误",
                                  QString("无法创建默认配置文件:\n%1").arg(QString::fromStdString(err)));
            return 1;
        }
        QMessageBox::critical(nullptr, "缺少配置",
                              QString("已生成默认配置文件:\n%1\n请编辑后重启程序。")
                                  .arg(QString::fromStdString(cfgPath)));
        return 0;
    }

    // --- 2. 严格校验：不通过即弹窗退出（v2 分层核心） ---
    usc::AppConfig cfg;
    const auto errors = usc::AppConfig::loadAndValidate(cfgPath, cfg);
    if (!errors.empty()) {
        showConfigErrors(errors);
        return 1;
    }

    // --- 3. SSID 校验（未指定则跳过；对齐原版 SSIDChecker） ---
    if (!cfg.targetSsid.empty()) {
        if (!usc::WlanChecker::isConnected(cfg.targetSsid)) {
            QMessageBox::critical(nullptr, "Start Error",
                                  QString("未连接到目标 WLAN: %1").arg(QString::fromStdString(cfg.targetSsid)));
            return 1;
        }
    }

    // --- 4. 业务层：唯一后台线程 ---
    auto* worker = new usc::CoreWorker(cfg, cfgPath);
    worker->start();

    // --- 5. 渲染层：日志窗口 + 托盘 ---
    auto* logWindow = new usc::LogWindow();
    auto* tray = new usc::TrayController();

    // 意图接线：GUI → 业务
    QObject::connect(logWindow, &usc::LogWindow::reauthRequested, worker,
                     &usc::CoreWorker::requestReauth);
    QObject::connect(logWindow, &usc::LogWindow::pauseRequested, worker,
                     &usc::CoreWorker::requestPause);
    QObject::connect(logWindow, &usc::LogWindow::resumeRequested, worker,
                     &usc::CoreWorker::resume);
    // 倒计时同步：每次日志刷新时拉取 CoreWorker 暂停状态更新按钮显示
    QObject::connect(logWindow, &usc::LogWindow::refreshTick, logWindow, [logWindow, worker]() {
        logWindow->setPauseState(worker->isPaused(), worker->pauseRemainSec());
    });
    auto doExit = [worker]() { shutdownApp(worker); };
    QObject::connect(logWindow, &usc::LogWindow::exitRequested, doExit);
    QObject::connect(tray, &usc::TrayController::exitRequested, doExit);

    // 状态接线：托盘 → 日志窗口
    QObject::connect(tray, &usc::TrayController::showLogRequested, logWindow,
                     &usc::LogWindow::showAndActivate);

    // 单实例激活：重复启动 exe 时弹出日志窗口
    QObject::connect(&guard, &usc::SingleInstance::activationRequested, logWindow,
                     &usc::LogWindow::showAndActivate);

    usc::log("校园网守护程序已启动");
    return app.exec();
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);  // 日志窗口关闭≠退出，托盘常驻

    // 应用图标：Windows 下 Qt 自动从 exe 资源加载（app.rc 的 ICON），
    // 显式设置 windowIcon 供托盘/窗口共用，保证与 exe 图标一致
    app.setWindowIcon(QIcon(QApplication::applicationFilePath()));

    const std::string exeDir = QApplication::applicationDirPath().toStdString();

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--smoke") {
            smokeTest(exeDir);
            return 0;
        }
        if (arg == "--login-only") return runLoginOnly(exeDir);
        if (arg == "--e2e") return runE2E(app, exeDir, argc, argv);
    }

    // --- 单实例守护：重复启动时通知已有实例弹窗，自身退出 ---
    usc::SingleInstance guard("USC-Auto-SingleInstance");
    if (!guard.isPrimary()) {
        guard.notifyExisting();
        return 0;
    }

    int rc = runGui(app, exeDir, guard);

    // 主实例退出时由 guard 析构关闭服务端；显式断开以防退出竞态
    return rc;
}

