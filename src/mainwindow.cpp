#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>
#include <QThread>
#include <QCoreApplication>
#include <windows.h>
#include <tlhelp32.h>
#include <QRegularExpression>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("尺寸测量报告集成软件");
    LoadCsvFileToUi(targetPath);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_generateReportBt_clicked()
{
    // 1. 创建尺寸报告实例
   DimReport dimReport;
   QString appDir = QCoreApplication::applicationDirPath();
   // 拼接模板绝对路径
   QString templateAbsolutePath = QDir(appDir).filePath("word_template/测试报告2.docx");
   // 校验模板是否存在（Qt 层面）
   if (!QFile::exists(templateAbsolutePath))
   {
       QMessageBox::warning(this, "错误", QString("模板不存在：%1").arg(templateAbsolutePath));
       return;
   }
   // 2. 设置模板路径和保存路径
   dimReport.setTemplatePath(templateAbsolutePath);

   QString timeStr = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

   QString savePath = QDir(appDir).filePath(QString("生成报告/%1_测试报告.docx").arg(timeStr));
   dimReport.setSavePath(savePath);
   // 3. 设置产品参数（包含一批产品的公共仪器信息，只需设置一次）
   DimReport::ProductParam prodParam;
   prodParam.jobOrder = "测试工作零号";
   prodParam.materialGrade = "测试材料";
   prodParam.customer = "测试客户";
   prodParam.productSerialNo = "测试产品号";
   // 公共仪器信息：一批产品统一，只需填写一次
   prodParam.MeasurementTool = "测试检测工具";
   prodParam.MeasurementNo = "测试检测工具号";
   dimReport.setProductParam(prodParam);
   // 4. 解析CSV参数（直接获取QList，无需转换）
   QList<DimReport::InspectionParam> paramList = buildParamMapFromCsv();
   if (paramList.isEmpty()) {
       QMessageBox::warning(this, "警告", "未解析到有效检测参数，无法生成报告！");
       return;
   }

   // 直接遍历QList添加参数（或如果DimReport有批量接口，可直接传递）
   for (const auto &singleParam : paramList) {
       dimReport.setInspectParam(singleParam);
   }


   dimReport.GenerateReport();

   // 6. 清空参数（保持不变）
   dimReport.clearInspectionParams();

}

void MainWindow::LoadCsvFileToUi(const QString &filePath)
{
    ui->testRetBox->clear();
    QDir targetDir(filePath);
    if (!targetDir.exists()) {
        QMessageBox::warning(nullptr, "警告", QString("CSV目录不存在：%1").arg(filePath));
        return;
    }

    QStringList csvFilters;
    csvFilters << "*.csv";
    QStringList csvFileNames = targetDir.entryList(csvFilters, QDir::Files, QDir::Name);
    if (!csvFileNames.isEmpty()) {
        ui->testRetBox->addItems(csvFileNames);
    } else {
        QMessageBox::warning(nullptr, "警告", QString("CSV目录下无CSV文件：%1").arg(filePath));
    }
}

QList<DimReport::InspectionParam> MainWindow::buildParamMapFromCsv()
{
    QList<DimReport::InspectionParam> paramList; // 直接用QList存储
    QString csvFileName = ui->testRetBox->currentText();
    if (csvFileName.isEmpty()) {
        QMessageBox::warning(nullptr, "错误", "请选择CSV文件！");
        return paramList;
    }

    QString csvFilePath = QDir(targetPath).filePath(csvFileName);
    QFile csvFile(csvFilePath);
    if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, "错误", QString("无法打开CSV文件：%1").arg(csvFilePath));
        return paramList;
    }

    QTextStream in(&csvFile);
    QStringList csvLines;
    while (!in.atEnd() && csvLines.size() < 2) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            csvLines.append(line);
        }
    }
    csvFile.close();

    if (csvLines.size() < 2) {
        QMessageBox::warning(nullptr, "错误", QString("CSV格式异常！仅读取到%1行，要求固定2行（表头+数据）").arg(csvLines.size()));
        return paramList;
    }

    QStringList headerColumns = csvLines[0].split(",");
    QStringList dataColumns = csvLines[1].split(",");

    QRegularExpression headerRegex("^([a-zA-Z]+)_(.*)$");
    QMap<QString, QMap<QString, QString>> paramAttrMap; // 这一步暂时保留，用于组装参数属性

    for (int colIndex = 0; colIndex < headerColumns.size(); ++colIndex)
    {
        QString header = headerColumns[colIndex].trimmed();
        QString data = (colIndex < dataColumns.size()) ? dataColumns[colIndex].trimmed() : "";

        QRegularExpressionMatch match = headerRegex.match(header);
        if (!match.hasMatch()) continue;

        QString attrType = match.captured(1);
        QString paramName = match.captured(2);
        paramAttrMap[paramName][attrType] = data;
    }

    auto convertToVariant = [](const QString& str) -> QVariant {
        if (str.compare("N/A", Qt::CaseInsensitive) == 0) return QVariant("N/A");
        bool isNumber;
        double num = str.toDouble(&isNumber);
        if (isNumber && qFuzzyCompare(num, 7777777.0)) return QVariant("无效值");
        return str.isEmpty() ? QVariant("") : (isNumber ? QVariant(num) : QVariant(str));
    };

    // 遍历属性Map，组装成InspectionParam并加入QList
    for (auto attrMapIt = paramAttrMap.begin(); attrMapIt != paramAttrMap.end(); ++attrMapIt)
    {
        QString paramName = attrMapIt.key();
        QMap<QString, QString> attrMap = attrMapIt.value();

        DimReport::InspectionParam param;
        param.name = paramName;
        param.defaultValue = convertToVariant(attrMap.value("DefaultValue", ""));
        param.maxValue = convertToVariant(attrMap.value("Max", ""));
        param.minValue = convertToVariant(attrMap.value("Min", ""));
        param.actualValue = convertToVariant(attrMap.value("ActualValue", ""));
        param.offset = convertToVariant(attrMap.value("Offset", ""));
        param.overOffset = convertToVariant(attrMap.value("OverOffset", ""));

        paramList.append(param); // 直接加入QList，无需Map存储
    }

    qDebug() << QString("CSV解析成功！共提取%1个检测参数").arg(paramList.size());
    return paramList;
}

