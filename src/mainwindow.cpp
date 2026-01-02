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
    InitCtrl();
    LoadQss::Load(":/qss/main.qss",this);
    LoadCsvFileToUi(targetPath);

}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::syncButtonStateWithDbStatus()
{
   // 获取当前数据库连接状态
   bool isDbAvailable = SqlService::Get().isAvailable();

   ui->dbConnectBt->setEnabled(!isDbAvailable);
   ui->dbdisConnectBt->setEnabled(isDbAvailable);

   // 同步连接按钮文本
   ui->dbConnectBt->setText(isDbAvailable ? "已连接" : "连接");
}
void MainWindow::InitCtrl()
{
    //绑定日志控件
    WidgetAppender*widgetAppender=new WidgetAppender();
    widgetAppender->bindWidget(ui->logEdit);
    LoggerManager::Get().registerAppender(widgetAppender);

    //初始化界面
    dbSetForm=new cell_Dbsetting(this);

    //设置数据库
    ui->dbIPBox->setEditable(true);

    mysqlConfig=&ConfigManager::Get().getConfig<MysqlConfig>();
    SqlService::Get().setConfig(*mysqlConfig);
    QList<QString>list=mysqlConfig->getPresetIps();
    ui->dbIPBox->addItems(list);
    syncButtonStateWithDbStatus();

    //连接数据库
    on_dbConnectBt_clicked();
    //根据工作令号-查询产品数据
    QueryProuductData();

    //报告类型
    QString templatePath="word_template";
    LoadReportType(templatePath);

}

bool MainWindow::QueryProuductData()
{
      // 1. 数据库连接检查
    if(!SqlService::Get().isAvailable())
    {
        LOG_ERROR("数据库未连接，无法查询工艺信息");
        ui->joborderCombox->clear();
        m_jobOrderToRecordMap.clear(); // 清空类成员变量
        return false;
    }
   //字段映射
    const QMap<QString, QString> fieldToHeaderMap = {
           {"product_id", "产品ID"},
           {"product_name", "产品名称"},
           {"job_order_no", "工作令号"},
           {"material_grade", "材质"},
           {"customer_po", "客户单号"},
           {"part_no", "零件号"},
           {"product_serial_no", "产品序列号"},
           {"drawing_no", "图号"},
           {"part_description", "零件描述"},
           {"smelting_furnace_no", "冶炼炉号"},
           {"heat_treatment_furnace_no", "热处理炉号"},
           {"heat_treatment_state", "热处理状态"},
           {"client_name", "委托单位"},
           {"quantity", "产品数量"},
           {"reviewer_result", "审核结果"},
           {"tool_name", "测量工具名称"},
           {"editor_name", "编制人员"},
           {"editor_opinion", "编制人员意见"},
           {"reviewer_name", "审核人员"},
           {"reviewer_opinion", "审核人员意见"},
           {"detection_standard_code", "检测标准号"},
           {"acceptance_standard_code", "验收标准号"},
           {"create_time", "年月日"}
    };
     // 2. 构造SQL并执行查询
    const QString TableName="product_base_info";
    // 查询产品基础表
    QString productSql = QString(R"(
               SELECT
                   p.product_id,
                   p.product_name,
                   p.job_order_no,
                   p.material_grade,
                   p.customer_po,
                   p.part_no,
                   p.product_serial_no,
                   p.drawing_no,
                   p.part_description,
                   p.smelting_furnace_no,
                   p.heat_treatment_furnace_no,
                   p.heat_treatment_state,
                   p.client_name,
                   p.quantity,
                   p.reviewer_result,
                   m.tool_name, -- 关联测量工具表
                   m.tool_no,
                   p.editor_name,
                   p.editor_opinion,
                   p.reviewer_name,
                   p.reviewer_opinion,
                   d.standard_code AS detection_standard_code, -- 关联检测标准表
                   a.acceptance_code AS acceptance_standard_code, -- 关联验收标准表
                   DATE_FORMAT(p.create_time, '%Y-%m-%d %H:%i:%s') AS create_time
               FROM %1 p
               LEFT JOIN measurement_tool m ON p.tool_id = m.tool_id
               LEFT JOIN detection_standard d ON p.standard_id = d.standard_id
               LEFT JOIN acceptance_standard a ON p.acceptance_id = a.acceptance_id
               ORDER BY  p.create_time ASC
           )").arg(TableName);
       // 执行查询
       SqlService::QueryResult productResult = SqlService::Get().GetData(productSql);
       if (!productResult.success)
       {
           LOG_ERROR(QString("加载产品基础数据失败：%1").arg(productResult.errorMsg));
           QMessageBox::critical(this, "错误", QString("加载数据失败：%1").arg(productResult.errorMsg));
           return false;
       }
       int rowCount = productResult.data.size();
       LOG_INFO(QString("产品基础信息查询到%1行记录").arg(rowCount));
        // 3. 清空旧数据
       ui->joborderCombox->clear();
       m_jobOrderToRecordMap.clear();
       if (rowCount == 0) {
           LOG_INFO("产品表暂无数据");
           return false;
       }
       // 5. 遍历QVariantMap类型的结果：
       QStringList jobOrderList;
       for (const QVariantMap &record : productResult.data)
       {
           QString jobOrderNo = record.value("job_order_no").toString().trimmed();
           if (jobOrderNo.isEmpty()) {
               continue;
           }
           jobOrderList.append(jobOrderNo);
           m_jobOrderToRecordMap.insert(jobOrderNo, record);
       }
       ui->joborderCombox->addItems(jobOrderList);
       return true;
}

void MainWindow::GetProductParams(DimReport::ProductParam *params)
{
    if(params)
    {
        QString currentJobOrder=ui->joborderCombox->currentText();

        params->jobOrder = m_jobOrderToRecordMap[currentJobOrder].value("job_order_no").toString();
        params->materialGrade =  m_jobOrderToRecordMap[currentJobOrder].value("material_grade").toString();
        params->customer = m_jobOrderToRecordMap[currentJobOrder].value("customer_po").toString();
        params->productSerialNo = m_jobOrderToRecordMap[currentJobOrder].value("product_serial_no").toString();
        params->MeasurementTool = m_jobOrderToRecordMap[currentJobOrder].value("tool_name").toString();
        params->MeasurementNo = m_jobOrderToRecordMap[currentJobOrder].value("tool_no").toString();
        params->reviewName=m_jobOrderToRecordMap[currentJobOrder].value("reviewer_name").toString();
        params->Inspector=m_jobOrderToRecordMap[currentJobOrder].value("editor_name").toString();
    }
    else
    {
        LOG_ERROR("获取报告基本信息失败");
    }



}
void MainWindow::LoadCsvFileToUi(const QString &filePath)
{
    LOG_INFO("正在读取检测数据文件......");
    ui->testfileCombox->clear();
    QDir targetDir(filePath);
    if (!targetDir.exists())
    {
        LOG_WARN(QString("Csv目录不存在").arg(filePath));
        return;
    }

    QStringList csvFilters;
    csvFilters << "*.csv";
    QStringList csvFileNames = targetDir.entryList(csvFilters, QDir::Files, QDir::Name);
    if (!csvFileNames.isEmpty())
    {
        ui->testfileCombox->addItems(csvFileNames);
        LOG_INFO(QString("读取到检测数据csv文件%1个").arg(csvFileNames.size()));
    }
    else
    {
          LOG_WARN(QString("CSV目录下无CSV文件：%1").arg(filePath));
    }
}
void MainWindow::LoadReportType(const QString &filePath)
{
    QStringList wordTemplate{
//      "迪威尔加工件尺寸检测记录",
        "客户2_尺寸检测报告.docx"
    };
    ui->reportTypeCombox->addItems(wordTemplate);

//    LOG_INFO("正在读取报告模板......");
//    ui->reportTypeCombox->clear();
//    QDir targetDir(filePath);
//    if (!targetDir.exists())
//    {
//        LOG_WARN(QString("报告目录不存在").arg(filePath));
//        return;
//    }

//    QStringList fileFilters;
//    fileFilters << "*.docx";
//    QStringList fileNames = targetDir.entryList(fileFilters, QDir::Files, QDir::Name);
//    if (!fileNames.isEmpty())
//    {
//        ui->reportTypeCombox->addItems(fileNames);
//        LOG_INFO(QString("读取到检测报告模板%1个").arg(fileNames.size()));
//    }
//    else
//    {
//          LOG_WARN(QString("word_template目录下无docx文件：%1").arg(filePath));
//    }
}
QList<DimReport::InspectionParam> MainWindow::buildParamMapFromCsv()
{
    QList<DimReport::InspectionParam> paramList; // 直接用QList存储
    QString csvFileName = ui->testfileCombox->currentText();
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





void MainWindow::on_dbConnectBt_clicked()
{
    bool connectSuccess = SqlService::Get().connectDb(); // 假设你有connectDb方法，返回连接结果

    if (connectSuccess)
    {
        LOG_INFO("数据库连接成功");
    }
    else
    {
        LOG_ERROR("数据库连接失败");
    }

    // 关键：连接操作后，同步按钮状态（实现互斥）
    syncButtonStateWithDbStatus();
}

void MainWindow::on_dbdisConnectBt_clicked()
{
   SqlService::Get().disconnectDb();

   // 同步按钮状态
   syncButtonStateWithDbStatus();

   // 日志输出（修正级别）
   if (SqlService::Get().isAvailable())
   {
       LOG_WARN("数据库断开失败"); // 断开失败用警告级别更合理
   }
   else
   {
       LOG_INFO("数据库断开成功"); // 正常结果用INFO级别
   }
}

void MainWindow::on_dbsetBt_clicked()
{
    if (dbSetForm)
    {
        dbSetForm->show();
        // 可选：设置为前端显示
        dbSetForm->raise();
        dbSetForm->activateWindow();
    }
}

void MainWindow::on_generateReportBt_clicked()
{
    // 1. 创建尺寸报告实例
   DimReport dimReport;
   QString appDir = QCoreApplication::applicationDirPath();

   QString templateName=ui->reportTypeCombox->currentText();

   // 拼接模板绝对路径
   QString templateAbsolutePath = QDir(appDir).filePath("word_template/%1").arg(templateName);
   // 校验模板是否存在
   if (!QFile::exists(templateAbsolutePath))
   {
       QMessageBox::warning(this, "错误", QString("模板不存在：%1").arg(templateAbsolutePath));
       return;
   }
   // 2. 设置模板路径和保存路径
   dimReport.setTemplatePath(templateAbsolutePath);

   QString timeStr = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

   QString savePath = QDir(appDir).filePath(QString("生成报告/%1_%2.docx").arg(timeStr).arg(templateName));
   dimReport.setSavePath(savePath);

   // 3. 设置产品参数
   DimReport::ProductParam prodParam;
   GetProductParams(&prodParam);
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







void MainWindow::on_queryRecordBt_clicked()
{
    if(!SqlService::Get().isAvailable())
    {
        LOG_ERROR("数据库未连接，无法查询工艺信息");
    }

    QueryProuductData();
}
