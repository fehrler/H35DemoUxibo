#ifndef HBSELECT_H
#define HBSELECT_H

#include <QWidget>

namespace Ui {
class HBselect;
}

class HBselect : public QWidget
{
    Q_OBJECT

public:
    explicit HBselect(QWidget *parent = 0);
    ~HBselect();
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *MouseEvent);
    void clear(bool MatrixDel, bool ConfigDel, bool empty);
    void ConfigToMatrix();
    void MatrixToConfig();
    void MatrixToConfig(int i, int j);

signals:
    void HBToMain(QVector<bool> HBConfig);

public slots:
    void MainToHB(QVector<bool> HBConfig);

private slots:
    void on_ClearSelection_clicked();

    void on_AcceptSelection_clicked();

    void on_AcceptSelection_2_clicked();

    void on_SelectAll_clicked();

private:
    Ui::HBselect *ui;
};

#endif // HBSELECT_H
