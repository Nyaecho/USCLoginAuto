// Logger.h — 环形日志缓冲（对齐 Python 版 CircularLogBuffer + LogWriter）
// 线程安全；超过上限截断保留 90%；自动加 [HH:MM:SS] 时间戳；GUI 通过 logAppended 信号感知新日志
#pragma once

#include <QObject>
#include <QString>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>

namespace usc {

class Logger : public QObject {
    Q_OBJECT
public:
    // 全局单例：后台线程直接 Logger::instance().write(...)，GUI 线程订阅信号
    static Logger& instance();

    // 写一条日志（自动加时间戳）。线程安全。
    void write(const std::string& msg);
    // 字面量便捷重载（避免 std::string/QString 重载歧义）
    void write(const char* msg);
    // 便捷重载
    void write(const QString& msg);

    // 获取全部日志内容（GUI 渲染用）。线程安全。
    QString content() const;

    // 清空。线程安全。
    void clear();

    // 当前缓冲字节量（调试/测试用）
    std::size_t sizeBytes() const;

signals:
    // 注意：write() 可能从后台线程调用，Qt 自动队列投递到 GUI 线程
    void logAppended(const QString& formattedLine);

private:
    explicit Logger(QObject* parent = nullptr);
    ~Logger() override = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static QString currentTimestamp();  // [HH:MM:SS]

    mutable std::mutex m_mutex;
    std::deque<std::string> m_lines;          // 行缓冲（替代 Python 的 StringIO）
    std::size_t m_bytes = 0;                  // 当前总字节数（UTF-8）
    std::size_t m_maxBytes = 10 * 1024 * 1024;  // 10MB，对齐原版
};

// 便捷自由函数：usc::log("...")，等价 Python 的 print（已被 LogWriter 劫持的版本）
void log(const std::string& msg);
void log(const char* msg);
void log(const QString& msg);

}  // namespace usc
