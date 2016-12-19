#ifndef DIGSELECT_H
#define DIGSELECT_H

#include <QWidget>

namespace Ui {
class DigSelect;
}

class DigSelect : public QWidget
{
    Q_OBJECT

public:
    explicit DigSelect(QWidget *parent = 0);
    ~DigSelect();
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *MouseEvent);

public slots:
    void MainToDigSel(QVector<std::pair<unsigned int, unsigned int> > activepixels);

signals:
    void DigSelToMain(QVector<std::pair<unsigned int, unsigned int> >);

private slots:
    void on_selAll_clicked();

    void on_selNone_clicked();

    void on_discardButton_clicked();

    void on_acceptButton_clicked();

    void on_selLeft_clicked();

    void on_selRight_clicked();

private:
    Ui::DigSelect *ui;

    bool pixels[300][16];
};

#endif // DIGSELECT_H
