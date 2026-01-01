#ifndef TOOLMANAGER_H
#define TOOLMANAGER_H

// 必须包含的头文件（解决编译依赖）
#include <QObject>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QWidget>
#include <QMessageBox>
//QT       +=axcontainer
#include <QAxObject>
#include <QAxWidget>

#include"sqlservice.h"


// 数据库同步工具类（独立功能，内嵌在头文件）
class DbSyncTool
{
public:
    DbSyncTool() = default; // 简化构造函数
    ~DbSyncTool() = default;

    // 方法
    QString getFieldNameByColumn(int col, const QString& tableName);//返回列号的字段名

    void syncCellEditToDb(QStandardItem *editedItem, QStandardItemModel *model,
                          const QString& tableName, const QString& primaryKey,
                          QWidget *parent = nullptr);//单表更新


};


// 统一工具入口（单例）
class ToolManager : public QObject
{
    Q_OBJECT
    explicit ToolManager(QObject *parent = nullptr) : QObject(parent) {}
    ~ToolManager() override = default;
    ToolManager(const ToolManager&) = delete;
    ToolManager& operator=(const ToolManager&) = delete;
public:
    static ToolManager& Get() {
        static ToolManager instance;
        return instance;
    }

    // 获取数据库同步工具
    DbSyncTool& dbSync() { return m_dbSyncTool; }


private:
    DbSyncTool m_dbSyncTool;
};

#endif // TOOLMANAGER_H
