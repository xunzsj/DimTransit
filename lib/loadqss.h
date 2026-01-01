#ifndef LOADQSS_H
#define LOADQSS_H
#include<QString>
#include<QApplication>

class LoadQss
{
public:
    LoadQss();
    static  void Load(const QString &qssFilePath, QApplication &app);
    static  void Load(const QString &qssFilePath, QWidget *widget);
private:

    static QString readQssFile(const QString &filePath);
};

#endif // LOADQSS_H
