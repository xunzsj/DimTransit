#ifndef SQLSERVICE_H
#define SQLSERVICE_H

#include <QObject>
#include<QSqlDatabase>
#include<QSqlError>
#include<QSqlQuery>
#include<QSqlRecord>
#include<QList>
#include"iconfig.h"
class SqlService : public QObject
{
    Q_OBJECT
public:
    static SqlService &Get(){
        static SqlService instance;
        return instance;
    }
private:
    SqlService();
    ~SqlService() override;
    SqlService(const SqlService&) = delete; // 禁用拷贝
    SqlService& operator=(const SqlService&) = delete; // 禁用赋值
public:
    // 1. 仅设置/更新数据库配置
    void setConfig(const BaseMysqlConfig& config);
    // 2. 根据当前配置建立数据库连接
    bool connectDb();
    // 3. 主动断开数据库连接
    void disconnectDb();
    // 4. 判断是否已连接
    bool isAvailable();

    struct QueryResult {
        bool success;          // 是否成功
        QList<QVariantMap> data; // 查询结果（查数据用）
        QString errorMsg;      // 错误信息
        int affectedRows = 0;  // 受影响行数（增删改用）
    };
    QueryResult NonQuery(const QString &sql, const QList<QVariant>& params = QList<QVariant>());

    QueryResult NonQuery(const QString& sql); // 增/删/改
    QueryResult GetData(const QString& sql);  // 查表格数据

private:
    QString getUniqueConnName() const {
        QString dbName = m_dbName.isEmpty() ? "DEFAULT_DB" : m_dbName;
        return QString("SQL_CONN_%1_%2").arg(dbName).arg((quint64)this);
     }
private:
    QString m_dbName;          // 数据库实例名
    QSqlDatabase m_db;        // 数据库连接
    bool m_isConnected;        // 连接状态
    mutable QMutex m_mutex;    // 线程安全锁
    QString m_lastError;       // 错误信息
    // 数据库配置项（支持动态更新）
    QString m_host = "127.0.0.1";
    int m_port = 3306;
    QString m_user = "root";
    QString m_password = "123456"; // 若不同库密码不同，


};

#endif // SQLSERVICE_H
