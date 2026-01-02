#include "cell_dbsetting.h"
#include "ui_cell_dbsetting.h"
#include<QMessageBox>
#include"lib/loadqss.h"

cell_Dbsetting::cell_Dbsetting(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::cell_Dbsetting)
{
    ui->setupUi(this);
    this->setWindowTitle("数据库配置");
    LoadQss::Load(":/qss/dbset.qss",this);
    this->setWindowFlag(Qt::WindowMaximizeButtonHint);
    this->setWindowFlag(Qt::WindowMinimizeButtonHint);
    //加载ini文件到UI
    LoadConfigToUi();

}

cell_Dbsetting::~cell_Dbsetting()
{
    delete ui;
}

void cell_Dbsetting::LoadConfigToUi()
{
    try {

      MysqlConfig& mysqlConfig= ConfigManager::Get().getConfig<MysqlConfig>();
       ui->DatabaseEdit->setText(mysqlConfig.getDbName());
       ui->PortEdit->setText(QString::number(mysqlConfig.getPort())); // int -> QString
       ui->UserEdit->setText(mysqlConfig.getUsername());
       ui->PasswordEdit->setText(mysqlConfig.getPassword());
   }
    catch (const std::runtime_error& e)
    {
       // 捕获异常，避免程序崩溃，并提示错误
       QMessageBox::critical(this, "错误", QString("加载配置失败：%1").arg(e.what()));
    }
}



void cell_Dbsetting::on_confirmBtn_clicked()
{
    try {
        MysqlConfig& mysqlConfig= ConfigManager::Get().getConfig<MysqlConfig>();
        mysqlConfig.setDbName(ui->DatabaseEdit->text().trimmed());
        mysqlConfig.setPort(ui->PortEdit->text().toInt()); // QString -> int
        mysqlConfig.setUsername(ui->UserEdit->text().trimmed());
        mysqlConfig.setPassword(ui->PasswordEdit->text().trimmed());
        ConfigManager::Get().saveAllConfig();
        QMessageBox::information(this, "提示", "数据库配置保存成功！");
        this->close(); // 保存后关闭窗口
    } catch (const std::runtime_error& e) {
        QMessageBox::critical(this, "错误", QString("保存配置失败：%1").arg(e.what()));
    }
}

void cell_Dbsetting::on_closeBtn_clicked()
{
    this->close();
}

