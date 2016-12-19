#ifndef DISPLAYREADOUT_H
#define DISPLAYREADOUT_H

#include <QWidget>



namespace Ui {
class DisplayReadout;
}

class DisplayReadout : public QWidget
{
    Q_OBJECT

public:
    explicit DisplayReadout(QWidget *parent = 0);
    ~DisplayReadout();
    void paintEvent(QPaintEvent *event);
    void closeEvent(QCloseEvent *event);

private:
    Ui::DisplayReadout *ui;

    int pixels[300][16];

public slots:
    void MainToReadout(std::string filename);
    void LiveReadout(std::pair<std::string, bool> data);
    void DisplayReadoutRepaint();
    QVector<int>  DecodeStrings(std::string ReadoutBlocks, std::string TrigTS1s, std::string TrigTS2s, std::string ExtTimes, std::string TSLs, std::string ADDRLs, std::string TSRs, std::string ADDRRs, std::string TrigTSs, std::string EventTSs, std::string Delays);

signals:
    void DisplayReadoutClosed();

};

#endif // DISPLAYREADOUT_H
