#include "itool.h"


QString DbSyncTool::getFieldNameByColumn(int col, const QString &tableName)
{
    // 1. 执行DESC语句查询表结构
   QString descSql = QString("DESC %1").arg(tableName);
   SqlService::QueryResult descResult = SqlService::Get().GetData(descSql); // 适配你的SqlService单例调用

   // 2. 校验结果：查询失败/列号越界 → 返回空
   if (!descResult.success || col < 0 || col >= descResult.data.size()) {
       return "";
   }

   // 3. 返回对应列号的字段名
   return descResult.data[col].value("Field").toString();
}

void DbSyncTool::syncCellEditToDb(QStandardItem *editedItem, QStandardItemModel *model, const QString &tableName, const QString &primaryKey, QWidget *parent)
{
    // 1. 校验入参
    if (!editedItem || !model) {
        if (parent) QMessageBox::warning(parent, "提示", "入参无效，同步失败！");
        return;
    }
    // 2. 获取编辑位置和新值
    int row = editedItem->row();
    int col = editedItem->column();
    QString newValue = editedItem->text();
    // 3. 获取主键值（第0列，隐藏的主键列）
    QStandardItem* pkItem = model->item(row, 0);
    QString pkValue = pkItem ? pkItem->text().trimmed() : "";

    // 4. 获取编辑的字段名
    QString fieldName = getFieldNameByColumn(col, tableName);
    if (fieldName.isEmpty()) {
        if (parent) QMessageBox::warning(parent, "提示", "无法匹配字段名，同步失败！");
        editedItem->setText(editedItem->data(Qt::UserRole).toString());
        return;
    }

    QString updateSql = QString("UPDATE %1 SET %2 = ? WHERE %3 = ?")
                          .arg(tableName).arg(fieldName).arg(primaryKey);
    QList<QVariant> params;
    params << newValue << pkValue;

    // 6. 执行更新并处理结果
    SqlService::QueryResult result = SqlService::Get().NonQuery(updateSql, params);
    if (result.success)
    {
        editedItem->setData(newValue, Qt::UserRole); // 记录新值，用于失败恢复
    }
    else
    {
        if (parent) QMessageBox::warning(parent, "提示", QString("数据同步失败：%1").arg(result.errorMsg));
        editedItem->setText(editedItem->data(Qt::UserRole).toString());
    }
}




