#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QVariant>
#include"lib/reporttool.h"



QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_generateReportBt_clicked();

private:
    Ui::MainWindow *ui;
    const QString targetPath = "test_data";

    void LoadCsvFileToUi(const QString &filePath);
    QList<DimReport::InspectionParam> buildParamMapFromCsv();


};

#endif // MAINWINDOW_H
