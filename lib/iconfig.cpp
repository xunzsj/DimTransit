#include "iconfig.h"

void BaseMysqlConfig::loadConfig(QSettings *settings)
{
    if (!settings) return;
    settings->beginGroup(getSection());
    // 读取配置，无值则保留默认
    m_host = settings->value("Host", m_host).toString();
    m_port = settings->value("Port", m_port).toInt();
    m_username = settings->value("Username", m_username).toString();
    m_password = settings->value("Password", m_password).toString();
    m_dbName = settings->value("DbName", m_dbName).toString();
    settings->endGroup();
}

void BaseMysqlConfig::saveConfig(QSettings *settings)
{
    if (!settings) return;
    settings->beginGroup(getSection());
    settings->setValue("Host", m_host);
    settings->setValue("Port", m_port);
    settings->setValue("Username", m_username);
    settings->setValue("Password", m_password);
    settings->setValue("DbName", m_dbName);
    settings->endGroup();
}


void LoginConfig::loadConfig(QSettings *settings)
{
    QMutexLocker locker(&m_mutex);
    settings->beginGroup(getSection());
    m_lastAccount = settings->value("LastAccount", "").toString();
    m_lastPwdCipher = settings->value("LastPwdCipher", "").toString(); // 读密文
    m_lastRoleValue = settings->value("LastRoleValue", -1).toInt();
    settings->endGroup();
}

void LoginConfig::saveConfig(QSettings *settings)
{
    QMutexLocker locker(&m_mutex);
    settings->beginGroup(getSection());
    settings->setValue("LastAccount", m_lastAccount);
    settings->setValue("LastPwdCipher", m_lastPwdCipher); // 存密文
    settings->setValue("LastRoleValue", m_lastRoleValue);
    settings->endGroup();
}

QString LoginConfig::encrypt(const QString& plainText, const QString& key)
{
    if (plainText.isEmpty()) return "";
    QByteArray plainData = plainText.toUtf8();
    QByteArray keyData = key.toUtf8();
    QByteArray encryptData;
    // 异或加密核心逻辑
    for (int i = 0; i < plainData.size(); ++i) {
        encryptData.append(plainData[i] ^ keyData[i % keyData.size()]);
    }
    // 转Base64，避免乱码，方便存到Ini文件
    return encryptData.toBase64();
}

QString LoginConfig::decrypt(const QString &cipherText, const QString &key)
{
    if (cipherText.isEmpty()) return "";
    QByteArray cipherData = QByteArray::fromBase64(cipherText.toUtf8());
    QByteArray keyData = key.toUtf8();
    QByteArray decryptData;
    // 异或解密（和加密逻辑完全一致）
    for (int i = 0; i < cipherData.size(); ++i) {
        decryptData.append(cipherData[i] ^ keyData[i % keyData.size()]);
    }
    return QString::fromUtf8(decryptData);

}
