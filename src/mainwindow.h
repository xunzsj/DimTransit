#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QVariant>
#include"lib/reporttool.h"
#include"lib/loadqss.h"
#include"lib/iconfig.h"
#include"lib/sqlservice.h"
#include"lib/ilogger.h"
//界面
#include"cell_dbsetting.h"

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

    void on_dbConnectBt_clicked();

    void on_dbdisConnectBt_clicked();

    void on_dbsetBt_clicked();

    void on_generateReportBt_clicked();//生成检测报告

    void on_queryRecordBt_clicked();

private:
    Ui::MainWindow *ui;
    //界面+配置
    cell_Dbsetting *dbSetForm;
    MysqlConfig* mysqlConfig;

    // 同步按钮状态与数据库连接状态
    void syncButtonStateWithDbStatus();
    void InitCtrl();
    const QString targetPath = "test_data";
    bool QueryProuductData();
    void  GetProductParams(DimReport::ProductParam*params);
    void LoadCsvFileToUi(const QString &filePath);
    void LoadReportType(const QString &filePath);
    QList<DimReport::InspectionParam> buildParamMapFromCsv();
private:
      QMap<QString, QVariantMap> m_jobOrderToRecordMap;

};

#endif // MAINWINDOW_H
