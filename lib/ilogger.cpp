#include "ILogger.h"
#include <QCoreApplication>
#include <QDir>
#include<QDebug>
#include <QThread>
#include <QTextCodec>

// 格式器实现（无修改）
QString BasicLogFormatter::format(const LogContext& context) {
    QString timeStr = context.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr;
    switch (context.level) {
        case LogLevel::Debug: levelStr = "[DEBUG]"; break;
        case LogLevel::Info:  levelStr = "[INFO]";  break;
        case LogLevel::Warn:  levelStr = "[WARN]";  break;
        case LogLevel::Error: levelStr = "[ERROR]"; break;
        case LogLevel::Fatal: levelStr = "[FATAL]"; break;
    }
//    QString threadStr = QString("[Thread:%1]").arg(context.threadId);

    return QString("%1 %2 %3 ")
           .arg(timeStr, levelStr,context.message);
}

QString DeviceLogFormatter::format(const LogContext& context) {
    QString timeStr = context.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr;
    switch (context.level) {
        case LogLevel::Debug: levelStr = "[DEBUG]"; break;
        case LogLevel::Info:  levelStr = "[INFO]";  break;
        case LogLevel::Warn:  levelStr = "[WARN]";  break;
        case LogLevel::Error: levelStr = "[ERROR]"; break;
        case LogLevel::Fatal: levelStr = "[FATAL]"; break;
    }
    QString threadStr = QString("[Thread:%1]").arg(context.threadId);
    QString deviceStr = QString("[Device:%1]").arg(context.device.isEmpty() ? "Unknown" : context.device);

    return QString("%1 %2 %3 %4 %5")
           .arg(timeStr, deviceStr, levelStr, threadStr, context.message);
}

// ===================== 输出器基类实现（无修改） =====================
void LogAppender::append(const LogContext& context, const QString& formattedMsg) {
    QMutexLocker locker(&m_mutex);
    if (m_enabled) {
        doAppend(context, formattedMsg);
    }
}

// ===================== 控件输出器实现（无修改） =====================
WidgetAppender::WidgetAppender(QObject *parent) : LogAppender(parent) {
    connect(this, &WidgetAppender::signalAppendText,
            this, &WidgetAppender::onAppendText,
            Qt::QueuedConnection);
}

void WidgetAppender::bindWidget(QTextEdit* widget) {
    m_widget = widget;
    m_isPlainTextEdit = false;
}

void WidgetAppender::bindWidget(QPlainTextEdit* widget) {
    m_widget = widget;
    m_isPlainTextEdit = true;
}

void WidgetAppender::onAppendText(const QString& text) {
    if (!m_widget) return;
    if (m_isPlainTextEdit) {
        QPlainTextEdit* plainEdit = static_cast<QPlainTextEdit*>(m_widget);
        plainEdit->appendPlainText(text);
        QTextCursor cursor = plainEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        plainEdit->setTextCursor(cursor);
    } else {
        QTextEdit* textEdit = static_cast<QTextEdit*>(m_widget);
        textEdit->append(text);
        QTextCursor cursor = textEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        textEdit->setTextCursor(cursor);
    }
}

void WidgetAppender::doAppend(const LogContext& context, const QString& formattedMsg) {
    emit signalAppendText(formattedMsg);
}

// ===================== 文件输出器实现（无修改） =====================
FileAppender::FileAppender(const QString& customLogDir, QObject *parent)
    : LogAppender(parent) {
    if (!customLogDir.isEmpty()) {
        m_logDir = customLogDir;
    } else {
        m_logDir = getDefaultLogDir();
    }

    QDir dir;
    if (!dir.exists(m_logDir)) {
        dir.mkpath(m_logDir);
    }

    m_logFile.setFileName(getLogFileName());
    if (m_logFile.open(QIODevice::Append | QIODevice::Text)) {
        m_logStream.setDevice(&m_logFile);
        m_logStream.setCodec("UTF-8");
    }
    else
    {
        qWarning() << "日志文件打开失败：" << m_logFile.errorString();
    }
}

FileAppender::~FileAppender() {
    flush();
    m_logFile.close();
}

void FileAppender::flush() {
    QMutexLocker locker(&m_fileMutex);
    m_logStream.flush();
    m_logFile.flush();
}

QString FileAppender::getLogFileName() {
    QString timeStr = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("%1/%2_Log.txt").arg(m_logDir, timeStr);
}

QString FileAppender::getDefaultLogDir() {
    QString exeDir = QCoreApplication::applicationDirPath();
    return QString("%1/Log").arg(exeDir);
}

void FileAppender::doAppend(const LogContext& context, const QString& formattedMsg) {
    QMutexLocker locker(&m_fileMutex);
    if (!m_logFile.isOpen()) return;
    m_logStream << formattedMsg << endl;
    m_logStream.flush();
    m_logFile.flush();
}

// ===================== 日志管理器实现（关键修改：移除ConsoleAppender注册） =====================
LoggerManager::LoggerManager() {
    // 注册默认格式器
    registerFormatter("Basic", new BasicLogFormatter());
    registerFormatter("Device", new DeviceLogFormatter());
    setCurrentFormatter("Basic"); // 默认使用设备格式器

    // 仅注册文件输出器（移除了控制台输出器），控件输出器在MainWindow中注册
    registerAppender(new FileAppender());
}

LoggerManager::~LoggerManager() {
    QMutexLocker locker(&m_mutex);
    qDeleteAll(m_formatters);
    m_formatters.clear();
    qDeleteAll(m_appenders);
    m_appenders.clear();
}

void LoggerManager::registerFormatter(const QString& name, LogFormatter* formatter) {
    QMutexLocker locker(&m_mutex);
    if (m_formatters.contains(name)) {
        delete m_formatters[name];
    }
    m_formatters[name] = formatter;
}

void LoggerManager::setCurrentFormatter(const QString& name) {
    QMutexLocker locker(&m_mutex);
    if (m_formatters.contains(name)) {
        m_currentFormatter = m_formatters[name];
    }
}

void LoggerManager::registerAppender(LogAppender* appender) {
    QMutexLocker locker(&m_mutex);
    m_appenders.append(appender);
}

LogContext LoggerManager::buildContext(LogLevel level, const QString& device, const QString& message) {
    LogContext context;
    context.timestamp = QDateTime::currentDateTime();
    context.level = level;
    context.device = device;
    context.message = message;
    context.threadId = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16);
    return context;
}

void LoggerManager::log(LogLevel level, const QString& message) {
    log(level, "", message);
}

void LoggerManager::log(LogLevel level, const QString& device, const QString& message) {
    QMutexLocker locker(&m_mutex);
    if (!m_currentFormatter) return;

    LogContext context = buildContext(level, device, message);
    QString formattedMsg = m_currentFormatter->format(context);

    for (LogAppender* appender : m_appenders) {
        appender->append(context, formattedMsg);
    }
}

void LoggerManager::flushAllFileAppenders() {
    QMutexLocker locker(&m_mutex);
    for (LogAppender* appender : m_appenders) {
        if (FileAppender* fileAppender = dynamic_cast<FileAppender*>(appender)) {
            fileAppender->flush();
        }
    }
}
