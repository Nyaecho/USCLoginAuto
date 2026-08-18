// Logger.cpp — 环形日志缓冲实现
#include "Logger.h"

#include <QDateTime>

namespace usc {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger(QObject* parent) : QObject(parent) {}

QString Logger::currentTimestamp() {
    // 对齐 Python: time.strftime("%H:%M:%S")
    return QDateTime::currentDateTime().toString("hh:mm:ss");
}

void Logger::write(const std::string& msg) {
    if (msg.empty()) return;

    QString formatted = "[" + currentTimestamp() + "] " + QString::fromStdString(msg);
    // Python 版在 LogWriter.write 里补换行；这里统一由 content() 拼接时处理行界
    QString trimmed = formatted;
    while (trimmed.endsWith('\n') || trimmed.endsWith('\r')) trimmed.chop(1);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const std::string line = trimmed.toStdString();
        m_lines.push_back(line);
        m_bytes += line.size() + 1;  // +1 换行符

        // 超限截断：从头部丢弃，直到降到 90% 以下（对齐原版保留 90% 逻辑）
        if (m_bytes > m_maxBytes) {
            const std::size_t target = static_cast<std::size_t>(m_maxBytes * 0.9);
            while (m_bytes > target && m_lines.size() > 1) {
                m_bytes -= m_lines.front().size() + 1;
                m_lines.pop_front();
            }
        }
    }

    // 信号可能从任意线程发射，Qt 跨线程自动走队列连接 → GUI 安全
    emit logAppended(formatted);
}

void Logger::write(const char* msg) {
    if (msg == nullptr) return;
    write(std::string(msg));
}

void Logger::write(const QString& msg) {
    write(msg.toStdString());
}

QString Logger::content() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QString result;
    result.reserve(static_cast<int>(m_bytes));
    for (std::size_t i = 0; i < m_lines.size(); ++i) {
        result += QString::fromStdString(m_lines[i]);
        if (i + 1 < m_lines.size()) result += '\n';
    }
    return result;
}

void Logger::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lines.clear();
    m_bytes = 0;
}

std::size_t Logger::sizeBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bytes;
}

void log(const std::string& msg) {
    Logger::instance().write(msg);
}

void log(const char* msg) {
    if (msg == nullptr) return;
    Logger::instance().write(std::string(msg));
}

void log(const QString& msg) {
    Logger::instance().write(msg);
}

}  // namespace usc
