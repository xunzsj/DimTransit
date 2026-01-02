#ifndef CELL_DBSETTING_H
#define CELL_DBSETTING_H

#include <QDialog>
#include"lib/iconfig.h"

namespace Ui {
class cell_Dbsetting;
}

class cell_Dbsetting : public QDialog
{
    Q_OBJECT

public:
    explicit cell_Dbsetting(QWidget *parent = nullptr);
    ~cell_Dbsetting();

private slots:


    void on_confirmBtn_clicked();

    void on_closeBtn_clicked();

private:
    Ui::cell_Dbsetting *ui;
    void LoadConfigToUi();
};

#endif // CELL_DBSETTING_H
