#ifndef ICONFIG_H
#define ICONFIG_H
#include<QSettings>
#include<QCoreApplication>
#include<QMutex>
#include<QString>
#include<iostream>
#include<exception>
#include<QCryptographicHash>
///
/// \brief 抽象基类：所有设备配置类的统一接口
///
class IConfig
{
public:
    //加载配置（从Ini读取到内存）
    virtual void loadConfig(QSettings* settings)=0;
    //保存配置（从内存写入Ini）
    virtual void saveConfig(QSettings *settings)=0;
    // 虚析构函数：确保子类正确析构
    virtual~IConfig()=default;
    // 获取配置节（用于Ini文件的分组）
    virtual QString getSection() const = 0;
};
///
/// \brief 通用MySQL配置基类（解决重复问题）
/// 抽取两个MySQL配置类的公共逻辑，子类仅需指定默认数据库名
///
class BaseMysqlConfig : public IConfig
{
public:
    explicit BaseMysqlConfig(const QString& defaultDbName)
        : m_dbName(defaultDbName)
        , m_presetIps({"127.0.0.1", "192.168.1.172"}) // 预设IP列表（本地+默认服务器）
    {
        // 通用默认配置
        m_host = m_presetIps.first();
        m_port = 3306;
        m_username = "root";
        m_password = "123456";
    }
    // 统一实现加载配置
    void loadConfig(QSettings* settings) override;

    // 统一实现保存配置
    void saveConfig(QSettings *settings) override;

    // 统一的配置节规则
    QString getSection() const override
    {
        return QString("Mysql_%1").arg(m_dbName);
    }

    // 统一的配置信息格式化（调试验证用）
    QString getCurrentConfigInfo() const
    {
        return QString("MySQL配置 [库名：%1] - 主机：%2，端口：%3，用户名：%4，密码：%5")
                .arg(m_dbName).arg(m_host).arg(m_port).arg(m_username).arg(m_password);
    }
    QList<QString> getPresetIps() const { return m_presetIps; }
    // 获取当前IP（选预设/手动输的）
    QString getCurrentIp() const { return m_host; }
    // 设置当前IP（UI选/输后调用）
    void setCurrentIp(const QString& ip) { m_host = ip.trimmed(); }
    // 通用get/set接口
    QString getHost() const { return m_host; }
    void setHost(const QString& host) { m_host = host; }
    int getPort() const { return m_port; }
    void setPort(int port) { m_port = port; }
    QString getUsername() const { return m_username; }
    void setUsername(const QString& username) { m_username = username; }
    QString getPassword() const { return m_password; }
    void setPassword(const QString& password) { m_password = password; }
    QString getDbName() const { return m_dbName; }
    void setDbName(const QString& dbName) { m_dbName = dbName; }
protected:
    const QList<QString> m_presetIps;
    QString m_host;
    int m_port;
    QString m_username;
    QString m_password;
    QString m_dbName;
    mutable QMutex m_mutex;
};

///
/// \brief 硬度卡数据库配置（仅指定默认库名，复用基类逻辑）
///
class MysqlConfig : public BaseMysqlConfig
{
public:
    MysqlConfig() : BaseMysqlConfig("dimensioncard") {}
};
///
/// \brief  登录配置类（记忆账号、密码、选中的角色）
///
class LoginConfig:public IConfig
{
public:
    LoginConfig() {
       // 默认值
       m_lastAccount = "";
       m_lastPwdCipher = "";
       m_lastRoleValue = -1; // 默认访客
   }
   void loadConfig(QSettings* settings) override;
   // 保存配置（从内存写入Ini）
   void saveConfig(QSettings* settings) override;
   // 配置节（Ini文件中的分组名）
   QString getSection() const override {
       return "Login"; // Ini中会生成[Login]节
   }
   // 对外的get/set接口
   QString getLastAccount() const {
       QMutexLocker locker(&m_mutex);
       return m_lastAccount;
   }
   void setLastAccount(const QString& account) {
       QMutexLocker locker(&m_mutex);
       m_lastAccount = account;
   }

   QString getLastPassword() const {
       QMutexLocker locker(&m_mutex);
        return decrypt(m_lastPwdCipher); // 密文→明文
   }
   void setLastPassword(const QString& plainPassword){
       QMutexLocker locker(&m_mutex);
       m_lastPwdCipher = encrypt(plainPassword); // 明文→密文
    }

   int getLastRoleValue() const {
       QMutexLocker locker(&m_mutex);
       return m_lastRoleValue;
   }
   void setLastRoleValue(int value) {
       QMutexLocker locker(&m_mutex);
       m_lastRoleValue = value;
   }
public:
   static QString encrypt(const QString &plainText, const QString &key = "MyLoginKey_2025");
   static QString decrypt(const QString& cipherText, const QString& key = "MyLoginKey_2025");
private:
    QString m_lastAccount;    // 上次登录的账号
    QString  m_lastPwdCipher;   // 上次登录的密码
    int m_lastRoleValue;      // 上次选中的角色值
    mutable QMutex m_mutex;   // 线程安全锁
};


///
/// \brief  配置管理类
///
class ConfigManager
{
public:
    static ConfigManager&Get(){
        static ConfigManager instance;
        return instance;
    }
    ConfigManager(const ConfigManager&)=delete;
    ConfigManager& operator =(const ConfigManager&)=delete;
public:
    template<typename T>
      T& getConfig() {
          QMutexLocker locker(&m_mutex);
          // 遍历配置列表，找到对应类型的实例（dynamic_cast 类型安全）
          for (IConfig* config : m_configList) {
              if (T* targetConfig = dynamic_cast<T*>(config)) {
                  return *targetConfig;
              }
          }
          // 找不到时抛异常
          throw std::runtime_error(QString("Config type %1 not registered").arg(typeid(T).name()).toStdString());
      }

public:
    // 手动触发保存所有配置
   void saveAllConfig()
   {
       QMutexLocker locker(&m_mutex);
       for (IConfig* config : m_configList) {
            config->saveConfig(m_settings);
       }
       m_settings->sync(); // 强制写入文件，避免缓存
   }

private:
    ConfigManager()
    {
        QString  iniPath=QCoreApplication::applicationDirPath()+"/config.ini";
        m_settings=new QSettings(iniPath,QSettings::IniFormat);
        m_settings->setIniCodec("UTF-8");
        //加载设备
        m_configList.append(new MysqlConfig());
        //
        m_configList.append(new LoginConfig());
        // 统一加载所有配置
        for (IConfig* config : m_configList) {
            config->loadConfig(m_settings);
        }

    }
    ~ConfigManager()
    {
        // 析构时备份保存
        saveAllConfig();
        qDeleteAll(m_configList);
        delete m_settings;
    }
private:
    QSettings *m_settings;
    QMutex m_mutex;
    QList<IConfig*> m_configList;


};


#endif // ICONFIG_H
