#include "loadqss.h"
#include<QFile>
#include <QApplication>
#include <QWidget>
#include<QDebug>
LoadQss::LoadQss()
{

}

void LoadQss::Load(const QString &qssFilePath, QApplication &app)
{
    QString styleSheet = readQssFile(qssFilePath);
    if (!styleSheet.isEmpty())
    {
        app.setStyleSheet(styleSheet);
    }
}

void LoadQss::Load(const QString &qssFilePath, QWidget *widget)
{
    if (!widget)
    {
         qWarning() << "LoadQss::load: widget is null!";
         return;
     }
     QString styleSheet = readQssFile(qssFilePath);
     if (!styleSheet.isEmpty())
     {
         widget->setStyleSheet(styleSheet);
     }
}

QString LoadQss::readQssFile(const QString &filePath)
{
    QFile file(filePath);
   // 打开文件失败时输出错误信息
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qWarning() << "Failed to open QSS file:" << filePath
                   << "Error:" << file.errorString();
        return "";
    }
    // 读取文件内容
    QString content = file.readAll();
    file.close();
    return content;
}
