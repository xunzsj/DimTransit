#ifndef REPORTTOOL_H
#define REPORTTOOL_H

#include <QString>
#include <QVariant>
#include <QAxObject>
#include <QList>
#include <QObject>
#include <Windows.h>
#include<QThread>

// 抽象报告生成工具类
class ReportTool
{
public:
    virtual void GenerateReport() = 0;
    virtual ~ReportTool() {} // 虚析构函数，避免内存泄漏
};

// 通用的Word操作工具类（可以独立使用）
class WordReportHelper : public QObject
{
    Q_OBJECT
public:
    explicit WordReportHelper(QObject *parent = nullptr);
    ~WordReportHelper();

    // 通用的Word操作接口
    static bool initWordAppSimple(QAxObject *&wordApp, QAxObject *&doc,
                                    const QString &templatePath, QObject *parent = nullptr);
    static void killResidualWordProcess(DWORD wordPid);

    static void fillBookmark(QAxObject *doc, const QString &bookmarkName,
                            const QString &fillText);
    static QAxObject* findBookmarkRow(QAxObject *doc, const QString &bookmarkName);
    static void insertRowsBelow(QAxObject *rows, QAxObject *referenceRow, int count);
    static void cleanupWordObjects(QAxObject *&doc, QAxObject *&wordApp); // 改为引用传递
    static void saveAndClose(QAxObject *doc, QAxObject *wordApp,
                            const QString &savePath);
     static void fillTableCell(QAxObject *table, int row, int col, const QVariant &fillValue);
private:

};

// 尺寸检测报告类（继承抽象类）
class DimReport : public QObject, public ReportTool
{
    Q_OBJECT
    // 定义信号，解决UI线程安全问题（非主线程调用时通过信号通知主线程弹框）
    signals:
        void reportInfo(const QString &title, const QString &content);
        void reportError(const QString &title, const QString &content);
        void reportWarning(const QString &title, const QString &content);

public:
    // 公共结构体定义
    struct ProductParam {
        QString jobOrder;        // 工作令号
        QString materialGrade;   // 材料牌号
        QString customer;        // 客户名称
        QString productSerialNo; // 产品序列号
        QString MeasurementTool;
        QString MeasurementNo;
    };

    struct InspectionParam {
        QString name;          // 参数名（Dimension No#）
        QVariant defaultValue; // 默认值
        QVariant maxValue;     // 最大值（Spec上限）
        QVariant minValue;     // 最小值（Spec下限）
        QVariant actualValue;  // 实际测量值
        QVariant offset;       // 偏移量
        QVariant overOffset;   // 超偏移量
    };

public:
    explicit DimReport(QObject *parent = nullptr);
    ~DimReport() override;

    // 实现纯虚函数：生成尺寸报告
    void GenerateReport() override;

    // 数据设置接口
    void setInspectParam(const InspectionParam &param) { m_paramList.append(param); }
    void setProductParam(const ProductParam &param) { m_productParam = param; }
    void setTemplatePath(const QString &path) { m_templatePath = path; }
    void setSavePath(const QString &path) { m_savePath = path; }
    void clearInspectionParams() { m_paramList.clear(); }

private:
    // 具体的报告填充实现（可被子类重写）
    virtual void fillReportContent(QAxObject *doc);
    virtual void fillTableData(QAxObject *doc);
    virtual void fillProductInfo(QAxObject *doc);

protected:
    QList<InspectionParam> m_paramList; // 存储所有检测参数
    ProductParam m_productParam;        // 存储产品参数
    QString m_templatePath;             // Word模板路径
    QString m_savePath;                 // 生成的报告保存路径

private:
    QAxObject *wordApp = nullptr; // Word实例
    DWORD m_wordPid = 0;
};

#endif // REPORTTOOL_H
