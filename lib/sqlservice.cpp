#include "sqlservice.h"
#include<QDebug>

SqlService::SqlService()
{
    QString connName = getUniqueConnName();
    if (QSqlDatabase::contains(connName))
    {
        m_db = QSqlDatabase::database(connName);
    }
    else
    {
        m_db = QSqlDatabase::addDatabase("QMYSQL", connName);
    }
    // 初始状态为未连接
    m_isConnected = false;
}

SqlService::~SqlService()
{
    QMutexLocker locker(&m_mutex); // 加锁防止析构时仍有操作
    if (m_isConnected && m_db.isOpen()) {
        m_db.close();
    }
    // 移除连接
    QString connName = getUniqueConnName();
    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase::removeDatabase(connName);

    }
    try {
           MysqlConfig& mysqlConfig = ConfigManager::Get().getConfig<MysqlConfig>();
           // 更新ConfigManager中的配置（与SqlService一致）
           mysqlConfig.setHost(m_host);
           mysqlConfig.setPort(m_port);
           mysqlConfig.setUsername(m_user);
           mysqlConfig.setPassword(m_password);
           mysqlConfig.setDbName(m_dbName);
           // 立即保存所有配置到ini文件
           ConfigManager::Get().saveAllConfig();
       } catch (const std::runtime_error& e) {
           m_lastError = QString("同步配置到ConfigManager失败：%1").arg(e.what());
           qWarning() << m_lastError;
       }

}

void SqlService::setConfig(const BaseMysqlConfig &config)
{
    QMutexLocker locker(&m_mutex); // 线程安全保护配置更新
    // 更新配置项
    m_host = config.getHost();
    m_port = config.getPort();
    m_user = config.getUsername();
    m_password = config.getPassword();
    m_dbName = config.getDbName();
}

bool SqlService::connectDb()
{
    QMutexLocker locker(&m_mutex);
    // 若已连接，先断开
    if (m_isConnected && m_db.isOpen()) {
        m_db.close();
        m_isConnected = false;
    }
    // 加载当前配置到数据库连接对象
    m_db.setHostName(m_host);
    m_db.setPort(m_port);
    m_db.setUserName(m_user);
    m_db.setPassword(m_password);
    m_db.setDatabaseName(m_dbName);
    // 执行连接
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        m_isConnected = false;
        return false;
    }
    // 连接成功
    m_isConnected = true;
    m_lastError.clear();
    return true;
}

void SqlService::disconnectDb()
{
    QMutexLocker locker(&m_mutex);
    // 若已连接，关闭数据库
    if (m_isConnected && m_db.isOpen()) {
        m_db.close();
        m_lastError.clear();
    }
    // 重置连接状态
    m_isConnected = false;
}


bool SqlService::isAvailable()
{
    QMutexLocker locker(&m_mutex);
    return m_isConnected;
}

SqlService::QueryResult SqlService::NonQuery(const QString &sql, const QList<QVariant> &params)
{
    QueryResult result;
    result.success = false;
    QMutexLocker locker(&m_mutex);
    if (!m_isConnected || !m_db.isOpen()) {
        result.errorMsg = "数据库未连接";
        return result;
    }
    QSqlQuery query(m_db);
    // 准备SQL（支持参数绑定）
    if (!query.prepare(sql)) {
        result.errorMsg = QString("SQL准备失败：%1（SQL：%2）").arg(query.lastError().text()).arg(sql);
        return result;
    }
    // 绑定参数（替换?占位符）
    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }

    // 执行SQL
    if (!query.exec()) {
        result.errorMsg = QString("NonQuery执行失败：%1（SQL：%2）").arg(query.lastError().text()).arg(sql);
        return result;
    }

    result.success = true;
    result.errorMsg = "";
    result.affectedRows = query.numRowsAffected();
    return result;
}

SqlService::QueryResult SqlService::NonQuery(const QString &sql)
{
    QueryResult result;
    result.success = false;
    QMutexLocker locker(&m_mutex);
    if (!m_isConnected || !m_db.isOpen()) {
        result.errorMsg = "数据库未连接";
        return result;
    }
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        result.errorMsg = QString("NonQuery执行失败：%1（SQL：%2）").arg(query.lastError().text()).arg(sql);
        return result;
    }
    // 执行成功
    result.success = true;
    result.errorMsg = "";
    result.affectedRows = query.numRowsAffected(); // 受影响行数
    return result;
}

SqlService::QueryResult SqlService::GetData(const QString &sql)
{
    QueryResult result;
    result.success = false;
    QMutexLocker locker(&m_mutex);
    if (!m_isConnected || !m_db.isOpen()) {
        result.errorMsg = "数据库未连接";
        return result;
    }
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        result.errorMsg = QString("GetData执行失败：%1（SQL：%2）").arg(query.lastError().text()).arg(sql);
        return result;
    }
    QSqlRecord record = query.record(); // 获取字段名信息
    while (query.next()) {
        QVariantMap rowMap;
        // 遍历所有字段，按字段名存储值
        for (int i = 0; i < record.count(); ++i) {
            QString fieldName = record.fieldName(i);
            QVariant fieldValue = query.value(i);
            rowMap.insert(fieldName, fieldValue);
        }
        result.data.append(rowMap);
    }
       // 4. 查询成功
    result.success = true;
    result.errorMsg = "";
    return result;
}
