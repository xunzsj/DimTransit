#ifndef ILOGGER_H
#define ILOGGER_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QDateTime>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QList>
#include <QMap>

// 日志等级（保留基础等级，无冗余逻辑）
enum class LogLevel {
    Debug,   // 调试信息
    Info,    // 普通信息
    Warn,    // 警告
    Error,   // 错误
    Fatal    // 致命错误
};

// 日志上下文（保留核心字段，简化无冗余）
struct LogContext {
    QDateTime timestamp;   // 时间戳（精确到毫秒）
    LogLevel level;        // 日志等级
    QString device;        // 设备名
    QString message;       // 日志内容
    QString threadId;      // 线程ID（调试用）
};

// 抽象格式器（定义日志格式，仅保留基础+设备两种）
class LogFormatter {
public:
    virtual ~LogFormatter() = default;
    virtual QString format(const LogContext& context) = 0;
};

// 基础格式器：时间戳 + 日志等级 + 信息
class BasicLogFormatter : public LogFormatter {
public:
    QString format(const LogContext& context) override;
};

// 设备格式器：时间戳 + 设备 + 日志等级 + 信息（默认使用）
class DeviceLogFormatter : public LogFormatter {
public:
    QString format(const LogContext& context) override;
};

// 抽象输出器（定义日志输出目标）
class LogAppender : public QObject {
    Q_OBJECT
public:
    explicit LogAppender(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~LogAppender() = default;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // 对外接口：线程安全的日志输出
    void append(const LogContext& context, const QString& formattedMsg);

protected:
    // 子类实现具体输出逻辑
    virtual void doAppend(const LogContext& context, const QString& formattedMsg) = 0;

private:
    bool m_enabled = true;
    mutable QMutex m_mutex; // 线程安全锁
};

// Qt控件输出器（实时输出到文本框，支持跨线程）
class WidgetAppender : public LogAppender {
    Q_OBJECT
public:
    explicit WidgetAppender(QObject *parent = nullptr);

    // 绑定Qt文本控件（QTextEdit/QPlainTextEdit）
    void bindWidget(QTextEdit* widget);
    void bindWidget(QPlainTextEdit* widget);

signals:
    // 跨线程更新控件的信号（确保实时且安全）
    void signalAppendText(const QString& text);

private slots:
    void onAppendText(const QString& text);

protected:
    void doAppend(const LogContext& context, const QString& formattedMsg) override;

private:
    QWidget* m_widget = nullptr;
    bool m_isPlainTextEdit = false;
};

// 文件输出器（支持自定义目录，默认：可执行文件同级/Log）
class FileAppender : public LogAppender {
    Q_OBJECT
public:
    // 构造函数：自定义目录（传空则使用默认目录）
    explicit FileAppender(const QString& customLogDir = "", QObject *parent = nullptr);
    ~FileAppender() override;

    // 手动刷盘（窗口关闭时调用，确保日志全部写入文件）
    void flush();

protected:
    void doAppend(const LogContext& context, const QString& formattedMsg) override;

private:
    // 生成文件名：yyyyMMdd_HHmmss_Log.txt（时间_Log格式）
    QString getLogFileName();
    // 获取默认日志目录：可执行文件同级/Log
    QString getDefaultLogDir();

private:
    QString m_logDir;       // 最终日志目录（自定义/默认）
    QFile m_logFile;
    QTextStream m_logStream;
    QMutex m_fileMutex;     // 文件操作锁
};

// 日志管理器（单例，简化无冗余）
class LoggerManager {
public:
    static LoggerManager& Get() {
        static LoggerManager instance;
        return instance;
    }

    // 禁止拷贝
    LoggerManager(const LoggerManager&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;

    // 注册格式器
    void registerFormatter(const QString& name, LogFormatter* formatter);
    // 设置当前格式器
    void setCurrentFormatter(const QString& name);

    // 注册输出器
    void registerAppender(LogAppender* appender);
    // 启用/禁用指定类型输出器
    template<typename T>
    void setAppenderEnabled(bool enabled) {
        QMutexLocker locker(&m_mutex);
        for (LogAppender* appender : m_appenders) {
            if (dynamic_cast<T*>(appender)) {
                appender->setEnabled(enabled);
            }
        }
    }

    // 核心日志接口（无设备名）
    void log(LogLevel level, const QString& message);
    // 核心日志接口（带设备名）
    void log(LogLevel level, const QString& device, const QString& message);

    // 手动刷盘所有文件输出器（窗口关闭时调用）
    void flushAllFileAppenders();

private:
    LoggerManager();
    ~LoggerManager();

    // 构建日志上下文
    LogContext buildContext(LogLevel level, const QString& device, const QString& message);

private:
    QMutex m_mutex;
    QMap<QString, LogFormatter*> m_formatters;
    LogFormatter* m_currentFormatter = nullptr;
    QList<LogAppender*> m_appenders;
};

// 便捷宏定义（简化调用）
#define LOG_DEBUG(msg) LoggerManager::Get().log(LogLevel::Debug, "", msg)
#define LOG_INFO(msg)  LoggerManager::Get().log(LogLevel::Info, "", msg)
#define LOG_WARN(msg)  LoggerManager::Get().log(LogLevel::Warn, "", msg)
#define LOG_ERROR(msg) LoggerManager::Get().log(LogLevel::Error, "", msg)
#define LOG_FATAL(msg) LoggerManager::Get().log(LogLevel::Fatal, "", msg)

#define LOG_DEVICE_DEBUG(device, msg) LoggerManager::Get().log(LogLevel::Debug, device, msg)
#define LOG_DEVICE_INFO(device, msg)  LoggerManager::Get().log(LogLevel::Info, device, msg)
#define LOG_DEVICE_WARN(device, msg)  LoggerManager::Get().log(LogLevel::Warn, device, msg)
#define LOG_DEVICE_ERROR(device, msg) LoggerManager::Get().log(LogLevel::Error, device, msg)
#define LOG_DEVICE_FATAL(device, msg) LoggerManager::Get().log(LogLevel::Fatal, device, msg)

#endif // ILOGGER_H
