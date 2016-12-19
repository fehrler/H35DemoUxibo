#ifndef GEBURTSTAG_H
#define GEBURTSTAG_H

#include <QWidget>

namespace Ui {
class Geburtstag;
}

class Geburtstag : public QWidget
{
    Q_OBJECT

public:
    explicit Geburtstag(QWidget *parent = 0);
    ~Geburtstag();

    void paintEvent(QPaintEvent *event);

private slots:
    void on_pushButton_clicked();

    void on_ClearButton_clicked();


public slots:
    void MainToGeburtstag(QVector<std::pair<unsigned int, unsigned int> > hitpixels);

signals:
    void GeburtstagtoMain(QVector<std::pair<unsigned int, unsigned int> > activepixels);

private:
    Ui::Geburtstag *ui;

    bool pixels[300][16];
};

#endif // GEBURTSTAG_H
