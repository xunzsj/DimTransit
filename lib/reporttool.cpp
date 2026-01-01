#include "reporttool.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QAxBase>
#include <QMessageBox>
#include <Windows.h>
#include <QApplication>
#include<QDateTime>

// 定义Word格式常量
const int WORD_FORMAT_DOCX = 16; // wdFormatXMLDocument (docx格式)

// ===================== WordReportHelper 实现 =====================
WordReportHelper::WordReportHelper(QObject *parent) : QObject(parent)
{

}

WordReportHelper::~WordReportHelper()
{

}

// 简化版本，只传递必要参数（修复资源泄漏问题）
bool WordReportHelper::initWordAppSimple(QAxObject *&wordApp, QAxObject *&doc,
                                        const QString &templatePath, QObject *parent)
{
    // 先初始化指针为空
    wordApp = nullptr;
    doc = nullptr;

    try {
        qDebug() << "使用简化方法初始化Word...";

        // 创建Word对象
        wordApp = new QAxObject("Word.Application", parent);
        if (wordApp->isNull()) {
            delete wordApp;
            wordApp = nullptr;
            return false;
        }

         wordApp->setProperty("Visible", false); // 保持可见，方便排查
          wordApp->setProperty("DisplayAlerts", 0); // 禁用所有弹窗

        QString cleanTemplatePath = templatePath.trimmed().remove(QChar('"'));
           if (!QFile::exists(cleanTemplatePath)) {
               qDebug() << "模板文件不存在：" << cleanTemplatePath;
               cleanupWordObjects(doc, wordApp);
               return false;
           }

       // 直接调用Word的Open方法
       QAxObject *documents = wordApp->querySubObject("Documents");
       if (!documents || documents->isNull()) {
           qDebug() << "获取Documents对象失败";
           if (documents) delete documents;
           cleanupWordObjects(doc, wordApp);
           return false;
       }

        // 使用最简参数：只传递文件名
        QVariant openResult = documents->dynamicCall(
                "Open(const QString&, bool, bool)",
                cleanTemplatePath, false, false
            );
        doc = nullptr;

        if (openResult.isNull() || !openResult.isValid()) {
            // 尝试使用ActiveDocument属性
            doc = wordApp->querySubObject("ActiveDocument");
            if (!doc || doc->isNull()) {
                qDebug() << "打开文档失败，ActiveDocument获取失败";
                delete documents;
                cleanupWordObjects(doc, wordApp);
                return false;
            }
        } else {
            // 从返回结果获取文档
            doc = qvariant_cast<QAxObject*>(openResult);
            if (!doc || doc->isNull()) {
                // 如果转换失败，尝试获取ActiveDocument
                doc = wordApp->querySubObject("ActiveDocument");
                if (!doc || doc->isNull()) {
                    qDebug() << "文档对象转换失败";
                    delete documents;
                    cleanupWordObjects(doc, wordApp);
                    return false;
                }
            }
        }

        delete documents;
        documents = nullptr;

        if (!doc || doc->isNull()) {
            qDebug() << "文档对象为空";
            cleanupWordObjects(doc, wordApp);
            return false;
        }

        qDebug() << "Word文档打开成功（简化方法）";
        return true;

    } catch (...) {
        qDebug() << "初始化Word时发生异常";
        cleanupWordObjects(doc, wordApp);
        return false;
    }
}

///
/// \brief WordReportHelper::killResidualWordProcess  关闭Word进程（增加容错处理）
/// \param wordPid
///
void WordReportHelper::killResidualWordProcess(DWORD wordPid)
{
    if (wordPid == 0) return;

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, wordPid);
    if (hProcess == NULL) {
        qWarning() << "获取Word进程句柄失败，无法终止残留进程（PID：" << wordPid << "）";
        return;
    }

    // 尝试正常终止，失败后再强制终止
    BOOL ret = TerminateProcess(hProcess, 0);
    if (!ret) {
        qWarning() << "终止Word进程失败（PID：" << wordPid << "）";
    } else {
        qDebug() << "Word残留进程已终止（PID：" << wordPid << "）";
    }

    CloseHandle(hProcess);
    hProcess = NULL;
}

///
/// \brief WordReportHelper::fillBookmark   查找书签并替换文本
/// \param doc
/// \param bookmarkName
/// \param fillText
///
void WordReportHelper::fillBookmark(QAxObject *doc, const QString &bookmarkName,
                                   const QString &fillText)
{
    if (!doc || doc->isNull() || bookmarkName.isEmpty()) return;

    QAxObject *bookmarks = doc->querySubObject("Bookmarks");
    if (!bookmarks || bookmarks->isNull()) {
        if (bookmarks) delete bookmarks;
        return;
    }

    bool exists = bookmarks->dynamicCall("Exists(const QString&)", bookmarkName).toBool();
    if (!exists) {
        qWarning() << "书签不存在：" << bookmarkName;
        delete bookmarks;
        return;
    }

    QAxObject *bookmark = bookmarks->querySubObject("Item(const QString&)", bookmarkName);
    if (!bookmark || bookmark->isNull()) {
        delete bookmark;
        delete bookmarks;
        return;
    }

    QAxObject *range = bookmark->querySubObject("Range");
    if (range && !range->isNull()) {
        range->setProperty("Text", fillText);
    }

    delete range;
    delete bookmark;
    delete bookmarks;
}

///
/// \brief WordReportHelper::findBookmarkRow  查找书签所在的行索引
/// \param doc
/// \param bookmarkName
/// \return
///
QAxObject* WordReportHelper::findBookmarkRow(QAxObject *doc, const QString &bookmarkName)
{
    if (!doc || doc->isNull() || bookmarkName.isEmpty()) {
        qDebug() << "findBookmarkRow：入参无效，返回null";
        return nullptr;
    }

    QAxObject *bookmark = nullptr;
    QAxObject *range = nullptr;
    QAxObject *row = nullptr;

    // 简化逻辑：直接获取书签 -> 范围 -> 行，减少临时对象
    try {
        // 1. 直接获取书签（跳过单独创建bookmarks对象，减少销毁带来的影响）
        bookmark = doc->querySubObject("Bookmarks(Item(const QString&))", bookmarkName);
        if (!bookmark || bookmark->isNull()) {
            qWarning() << "findBookmarkRow：书签" << bookmarkName << "不存在或无效";
            goto CLEANUP; // 直接清理返回
        }

        // 2. 获取书签范围
        range = bookmark->querySubObject("Range");
        if (!range || range->isNull()) {
            qWarning() << "findBookmarkRow：无法获取书签" << bookmarkName << "的范围";
            goto CLEANUP;
        }

        // 3. 直接获取范围对应的行（关键：绕过table/rows，直接获取行，减少兼容问题）
        row = range->querySubObject("Rows(1)");
        if (!row || row->isNull()) {
            qWarning() << "findBookmarkRow：书签" << bookmarkName << "不在表格行内";
            goto CLEANUP;
        }

        // 注意：此处不销毁 row（要返回给外部使用），只销毁临时对象
        delete range;
        delete bookmark;
        qDebug() << "findBookmarkRow：成功获取" << bookmarkName << "对应的行对象";
        return row;

    } catch (...) {
        qWarning() << "findBookmarkRow：查找书签行时发生致命异常";
        goto CLEANUP;
    }

CLEANUP:
    // 异常/失败时，销毁所有对象（包括row）
    if (row) delete row;
    if (range) delete range;
    if (bookmark) delete bookmark;
    return nullptr;
}
///
/// \brief WordReportHelper::insertRowsBelow 批量插入行（优化效率，移除休眠）
/// \param rows
/// \param referenceRow
/// \param count
///
void WordReportHelper::insertRowsBelow(QAxObject *rows, QAxObject *referenceRow, int count)
{
    if (count <= 0 || !rows || rows->isNull() || !referenceRow || referenceRow->isNull())
    {
        qDebug() << "insertRowsBelow：入参无效，跳过插入";
        return;
    }
    try
    {
        qDebug() << "批量插入" << count << "行数据（优化版）";

        for (int i = 0; i < count; ++i) {
            rows->dynamicCall( "Add(QAxObject*)",
                            QVariant::fromValue(referenceRow));
        }
        qDebug() << "行插入完成（优化版）";
    } catch (...) {
        qDebug() << "插入行失败！（优化版捕获异常）";
    }
}

///
/// \brief WordReportHelper::cleanupWordObjects 清理Word对象（改为引用传递，解决野指针问题）
/// \param doc
/// \param wordApp
///
void WordReportHelper::cleanupWordObjects(QAxObject *&doc, QAxObject *&wordApp)
{
    if (doc)
    {
       try
       {
           doc->dynamicCall("Close(bool)", false);  // 不保存
       }
        catch (...)
        {
            qWarning() << "关闭文档时发生异常";
        }
       delete doc;
       doc = nullptr; // 此时修改的是实参，可彻底置空
   }

   if (wordApp) {
       try {
           wordApp->dynamicCall("Quit()");
       } catch (...) {
           qWarning() << "退出Word时发生异常";
       }
       delete wordApp;
       wordApp = nullptr; // 此时修改的是实参，可彻底置空
   }
}

void WordReportHelper::saveAndClose(QAxObject *doc, QAxObject *wordApp,
                                   const QString &savePath)
{
    if (!doc || doc->isNull()) return;

    // 确保保存目录存在
    QFileInfo saveFileInfo(savePath);
    QDir saveDir = saveFileInfo.absoluteDir();
    if (!saveDir.exists()) {
        if (!saveDir.mkpath(".")) {
            qWarning() << "创建保存目录失败：" << saveDir.absolutePath();
            return;
        }
    }

    QString winSavePath = QDir::toNativeSeparators(savePath);
    // 使用兼容性更好的SaveAs接口
    doc->dynamicCall("SaveAs(const QString&, int)", winSavePath, WORD_FORMAT_DOCX);

    // 关闭文档和Word
    cleanupWordObjects(doc, wordApp);
}

// ======通用单元格填充实现（WordReportHelper 类中） ==========
void WordReportHelper::fillTableCell(QAxObject *table, int row, int col, const QVariant &fillValue)
{
    // 1. 前置参数有效性校验（避免无效操作导致崩溃）
    if (!table || table->isNull()) {
        qDebug() << "WordReportHelper::fillTableCell 失败：table 对象为空或无效";
        return;
    }
    if (row <= 0 || col <= 0) {
        qDebug() << "WordReportHelper::fillTableCell 失败：行号/列号无效（行：" << row << "，列：" << col << "）";
        return;
    }
    if (!fillValue.isValid()) {
        qDebug() << "WordReportHelper::fillTableCell 警告：填充值无效（行：" << row << "，列：" << col << "）";
        return;
    }

    QAxObject *cell = nullptr;
    QAxObject *cellRange = nullptr;

    // 2. COM操作包裹在try-catch中，防止致命异常
    try
    {
        // 获取表格指定单元格
        cell = table->querySubObject("Cell(int, int)", row, col);
        if (!cell || cell->isNull()) {
            qDebug() << "WordReportHelper::fillTableCell 失败：无法获取单元格（行：" << row << "，列：" << col << "）";
            goto CLEANUP;
        }

        // 获取单元格文本范围
        cellRange = cell->querySubObject("Range");
        if (!cellRange || cellRange->isNull()) {
            qDebug() << "WordReportHelper::fillTableCell 失败：无法获取单元格Range（行：" << row << "，列：" << col << "）";
            goto CLEANUP;
        }

        // 填充文本（统一转为字符串，兼容所有QVariant类型：数字、字符串等）
        QString textToFill = fillValue.toString().trimmed();
        cellRange->setProperty("Text", textToFill);
        qDebug() << "WordReportHelper::fillTableCell 成功：（行：" << row << "，列：" << col << "）填充值：" << textToFill;
    } catch (...) {
        qDebug() << "WordReportHelper::fillTableCell 异常：填充单元格时发生COM错误（行：" << row << "，列：" << col << "）";
    }

    // 3. 统一清理COM对象，避免内存泄漏
CLEANUP:
    if (cellRange)
    {
        delete cellRange;
        cellRange = nullptr;
    }
    if (cell)
    {
        delete cell;
        cell = nullptr;
    }
}

// ===================== DimReport 实现 =====================
DimReport::DimReport(QObject *parent)
    : QObject(parent), wordApp(nullptr), m_wordPid(0)
{

}

DimReport::~DimReport()
{
    WordReportHelper::killResidualWordProcess(m_wordPid);
    // 额外清理wordApp对象
    if (wordApp) {
        QAxObject *tmpDoc = nullptr;
        WordReportHelper::cleanupWordObjects(tmpDoc, wordApp);
    }
}

void DimReport::GenerateReport()
{
    qDebug() << "=== 开始生成报告 ===";

    // 1. 先清理路径引号
    QString cleanTemplatePath = m_templatePath.trimmed().remove(QChar('"'));
    QString cleanSavePath = m_savePath.trimmed().remove(QChar('"'));

    qDebug() << "模板路径(清理后):" << cleanTemplatePath;
    qDebug() << "保存路径(清理后):" << cleanSavePath;
    qDebug() << "检测参数数量:" << m_paramList.count();

    // 2. 生成临时副本路径（确保路径有效，权限足够）
    QString tempDir = QDir::tempPath(); // 系统临时目录，绝对有权限
    QString tempTemplateName = "temp_report_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".docx";
    QString tempTemplatePath = tempDir + "/" + tempTemplateName;

    // 3. 复制原模板到临时目录（先删除旧临时文件，避免冲突）
    if (QFile::exists(tempTemplatePath)) {
        QFile::remove(tempTemplatePath);
    }
    bool copyOk = QFile::copy(cleanTemplatePath, tempTemplatePath);
    if (!copyOk) {
        qWarning() << "复制模板到临时目录失败！尝试直接打开原模板";
        // 复制失败时，降级使用原模板
        tempTemplatePath = cleanTemplatePath;
    } else {
        qDebug() << "模板已复制到临时目录：" << tempTemplatePath;
    }

    // 4. 参数校验
    if (tempTemplatePath.isEmpty() || cleanSavePath.isEmpty())
    {
        QString title = "生成失败";
        QString content = "模板路径或保存路径未设置！";
        if (QThread::currentThread() == qApp->thread()) {
            QMessageBox::critical(nullptr, title, content);
        } else {
            emit reportError(title, content);
        }
        return;
    }

    if (m_paramList.isEmpty())
    {
        QString title = "提示";
        QString content = "无检测参数，跳过报告生成！";
        if (QThread::currentThread() == qApp->thread()) {
            QMessageBox::warning(nullptr, title, content);
        } else {
            emit reportWarning(title, content);
        }
        return;
    }

    // 5. 初始化Word（传入临时模板路径，用上面修正后的打开方式）
    QAxObject *doc = nullptr;
    qDebug() << "初始化Word应用程序...";
    if (!WordReportHelper::initWordAppSimple(wordApp, doc, tempTemplatePath, this)) {
        QString title = "错误";
        QString content = "无法启动Word应用程序！";
        if (QThread::currentThread() == qApp->thread()) {
            QMessageBox::critical(nullptr, title, content);
        } else {
            emit reportError(title, content);
        }
        return;
    }

    // 后续PID获取、填充内容逻辑不变...
    if (wordApp && !wordApp->isNull()) {
        try {
            QVariant pidVar = wordApp->property("ProcessId");
            if (pidVar.isValid() && pidVar.canConvert<DWORD>()) {
                m_wordPid = pidVar.value<DWORD>();
            } else {
                m_wordPid = 0;
            }
        } catch (...) {
            m_wordPid = 0;
        }
        qDebug() << "Word进程PID:" << m_wordPid;
    }

    qDebug() << "开始填充报告内容...";
    try
    {
        fillReportContent(doc);
        qDebug() << "报告内容填充完成，准备保存...";

        WordReportHelper::saveAndClose(doc, wordApp, cleanSavePath);
        wordApp = nullptr;
        doc = nullptr;

        QString title = "生成成功";
        QString content = QString("报告已保存至：\n%1").arg(cleanSavePath);
        if (QThread::currentThread() == qApp->thread()) {
            QMessageBox::information(nullptr, title, content);
        } else {
            emit reportInfo(title, content);
        }

    } catch (const std::exception &e) {
        qDebug() << "异常捕获:" << e.what();
        QString title = "生成失败";
        QString content = QString("生成报告时发生异常：%1").arg(e.what());
        if (QThread::currentThread() == qApp->thread()) {
            QMessageBox::critical(nullptr, title, content);
        } else {
            emit reportError(title, content);
        }
        WordReportHelper::cleanupWordObjects(doc, wordApp);
        wordApp = nullptr;
        doc = nullptr;

    } catch (...) {
        qDebug() << "未知异常";
        QString title = "生成失败";
        QString content = "生成报告时发生未知异常！";
        if (QThread::currentThread() == qApp->thread()) {
            QMessageBox::critical(nullptr, title, content);
        } else {
            emit reportError(title, content);
        }
        WordReportHelper::cleanupWordObjects(doc, wordApp);
        wordApp = nullptr;
        doc = nullptr;
    }
}

void DimReport::fillTableData(QAxObject *doc)
{
    qDebug() << "===== 开始执行 fillTableData 函数（插入行 + 通用类填充数据） =====";

    if (!doc || doc->isNull()) {
        qDebug() << "异常：doc 为空或无效，直接返回";
        return;
    }

    QAxObject *bookmark = nullptr;
    QAxObject *range = nullptr;
    QAxObject *rows = nullptr;
    QAxObject *targetRow = nullptr;
    QAxObject *table = nullptr;

    try
    {
        // 1. 获取书签
        bookmark = doc->querySubObject("Bookmarks(Item(const QString&))", "DataInsertPoint");
        if (!bookmark || bookmark->isNull())
        {
            qDebug() << "异常：未找到 DataInsertPoint 书签";
            goto CLEANUP;
        }
        qDebug() << "书签 DataInsertPoint 获取成功";

        // 2. 获取书签Range
        range = bookmark->querySubObject("Range");
        if (!range || range->isNull()) {
            qDebug() << "异常：无法获取书签 Range";
            goto CLEANUP;
        }
        qDebug() << "书签 Range 获取成功";

        // 3. 获取Rows
        rows = range->querySubObject("Rows");
        if (!rows || rows->isNull()) {
            qDebug() << "异常：书签 Range 无对应表格行（Rows）";
            goto CLEANUP;
        }
        qDebug() << "Rows 对象直接获取成功";

        // 4. 获取Table（用于数据填充，调用通用类方法）
        table = range->querySubObject("Tables(1)");
        if (!table || table->isNull()) {
            qDebug() << "异常：书签 Range 无对应表格（Table）";
            goto CLEANUP;
        }
        qDebug() << "Table 对象获取成功";

        // 5. 获取参考行
        targetRow = rows->querySubObject("First");
        if (!targetRow || targetRow->isNull()) {
            qDebug() << "异常：无法获取 Rows 第一行作为参考";
            goto CLEANUP;
        }
        qDebug() << "参考行（targetRow）获取成功，准备插入" << m_paramList.count() << "行";

        // 6. 获取参考行索引
        QVariant targetRowIndexVar = targetRow->property("Index");
        int targetRowIndex = -1;
        if (targetRowIndexVar.isValid() && targetRowIndexVar.canConvert<int>()) {
            targetRowIndex = targetRowIndexVar.toInt();
            qDebug() << "参考行索引：" << targetRowIndex << "，插入行将从" << (targetRowIndex + 1) << "开始";
        } else {
            qDebug() << "无法获取参考行索引，填充数据可能异常";
            goto CLEANUP;
        }

        // 7. 插入行
        WordReportHelper::insertRowsBelow(rows, targetRow, m_paramList.count()-1);
        qDebug() << "行插入操作执行完毕！开始调用通用类填充数据";

        // 8. 遍历参数，调用通用方法填充单元格（简化核心逻辑）
        for (int i = 0; i < m_paramList.count(); ++i)
        {
            int currentRow = targetRowIndex + i; // 计算当前行号
            const DimReport::InspectionParam &param = m_paramList.at(i);
            // 直接调用WordReportHelper静态方法，无需重复写填充逻辑
            WordReportHelper::fillTableCell(table, currentRow, 1, param.name); // 第1列：参数名
            WordReportHelper::fillTableCell(table, currentRow, 2, m_productParam.MeasurementTool); // 第2列：检测工具
            WordReportHelper::fillTableCell(table, currentRow, 3, m_productParam.MeasurementNo); // 第3列：检测工具号
            WordReportHelper::fillTableCell(table, currentRow, 4, param.defaultValue); // 第4列：默认值
            // 第5列：拼接实际值+偏移量
            QString actualWithOffset = QString("%1 %2").arg(param.actualValue.toString()).arg(param.offset.toString());
            WordReportHelper::fillTableCell(table, currentRow, 5, actualWithOffset);
        }
        qDebug() << "数据填充完毕！===== fillTableData 函数正常退出 =====";
    } catch (...) {
        qDebug() << "异常：fillTableData 执行过程中崩溃（COM操作失败）";
    }

CLEANUP:
    // 统一清理对象
    if (table) delete table;
    if (targetRow) delete targetRow;
    if (rows) delete rows;
    if (range) delete range;
    if (bookmark) delete bookmark;
    return;
}

void DimReport::fillReportContent(QAxObject *doc)
{
    if (!doc || doc->isNull()) return;
    fillProductInfo(doc);
    fillTableData(doc);
}

void DimReport::fillProductInfo(QAxObject *doc)
{
    if (!doc || doc->isNull()) return;

    // 使用工具类填充书签
    WordReportHelper::fillBookmark(doc, "JobOrderNo", m_productParam.jobOrder);
    WordReportHelper::fillBookmark(doc, "Customer", m_productParam.customer);
    WordReportHelper::fillBookmark(doc, "MaterialGrade", m_productParam.materialGrade);
    WordReportHelper::fillBookmark(doc, "ProductSerialNo", m_productParam.productSerialNo);
}
