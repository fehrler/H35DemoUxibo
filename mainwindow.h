#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "libs/geniobase.h"
#include "libs/ftdi.h"
#include "libs/nexysio.h"
#include "hbselect.h"
#include "digselect.h"
#include "geburtstag.h"
#include "displayreadout.h"
#include <QMediaPlayer>

//alglib stuff
#include "alglib/stdafx.h"
#include "alglib/optimization.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    bool HBenCol[120], HBenRow[40];
    static double CDF(double x, double x0, double width);

private slots:
    void on_Update_clicked();
    void logit(std::string logstream);


    void on_Slider_HBDig_valueChanged(int value);

    void on_Slider_PDelDig_valueChanged(int value);

    void on_Slider_NDelDig_valueChanged(int value);

    void on_Slider_PTrimDig_valueChanged(int value);

    void on_Slider_NCompDig_valueChanged(int value);

    void on_Slider_BLResDig_valueChanged(int value);

    void on_Slider_BLRes_valueChanged(int value);

    void on_Slider_NBiasRes_valueChanged(int value);

    void on_Slider_NFB_valueChanged(int value);

    void on_Slider_PTrim_valueChanged(int value);

    void on_Slider_NTWDown_valueChanged(int value);

    void on_Slider_NTW_valueChanged(int value);

    void on_Slider_NLogic_valueChanged(int value);

    void on_Slider_PAmpLoad_valueChanged(int value);

    void on_Slider_NSF_valueChanged(int value);

    void on_Slider_PAmp_valueChanged(int value);

    void on_Slider_PAB_valueChanged(int value);

    void on_spinBox_HBDig_valueChanged(int arg1);

    void on_spinBox_PDelDig_valueChanged(int arg1);

    void on_spinBox_NDelDig_valueChanged(int arg1);

    void on_spinBox_PTrimDig_valueChanged(int arg1);

    void on_spinBox_NCompDig_valueChanged(int arg1);

    void on_spinBox_BLResDig_valueChanged(int arg1);

    void on_spinBox_BLRes_valueChanged(int arg1);

    void on_spinBox_NBiasRes_valueChanged(int arg1);

    void on_spinBox_NFB_valueChanged(int arg1);

    void on_spinBox_PTrim_valueChanged(int arg1);

    void on_spinBox_NTWDown_valueChanged(int arg1);

    void on_spinBox_NTW_valueChanged(int arg1);

    void on_spinBox_NLogic_valueChanged(int arg1);

    void on_spinBox_PAmpLoad_valueChanged(int arg1);

    void on_spinBox_NSF_valueChanged(int arg1);

    void on_spinBox_PAmp_valueChanged(int arg1);

    void on_spinBox_PAB_valueChanged(int arg1);

    void on_Injection_clicked();

    void on_Start_Pattern_clicked();

    void on_timer_update();

    void on_SearchDevices_clicked();

    void on_OpenDevice_clicked();

    void on_Devices_currentIndexChanged(int index);

    void on_CloseDevice_clicked();

    void on_SendPair_clicked();

    void GetASICConfig(int Matrix);

    void GetPCBConfig();

    void on_Update_2_clicked();

    void on_MatrixSelect_currentIndexChanged(int index);

    void SetASICConfig(int Matrix);

    void on_scanButton_clicked();

    void on_openButton_clicked();

    void on_histogramButton_clicked();

    void on_GetFullMatrix_clicked();

    void on_coincidenceButton_clicked();

    bool getwaveform(std::string suffix, bool save);

    QVector<std::pair<double, double> > getwaveform(int channel, int lengtha);

    void on_InjectionButton_clicked();

    float getmax(std::string myfile);

    float getmin(std::string myfile);

    void SendConfig(bool ModeRamPix, bool ModeRamDig, int LdPixIndex, int LdDigIndex, bool LdPixEn, bool LdDigEn);

    void on_Hitbus_clicked();

    void on_WriteRamDig_clicked();

    void on_DigSel_clicked();

    void on_ReadoutButton_clicked();

    void on_FastReadoutEnable_clicked();

    void on_FastReadoutReset_clicked();

    void on_IPEGeburtstagsButton_clicked();

    //double ThresholdScan(double tolerance);

    double ThresholdScanV2(double tolerance);

    void on_H35Col_Meas_valueChanged(int arg1);

    void on_H35Row_Meas_valueChanged(int arg1);

    void on_H35Col_valueChanged(int arg1);

    void on_H35Row_valueChanged(int arg1);

    QVector<float> get_efficiency(double injection, int precision);

    void on_Thresholdall_Button_clicked();

    void on_TrimRamPix_valueChanged(int arg1);

    void set_ProgressBar(int max, int now, bool visible);

    void setRam(double Thmax, double Thmin);

    void on_Write_RamPix_Button_clicked();

    void on_Clear_RamPix_Button_clicked();

    void findRam(double tolerance);

    void on_TrimScan_clicked();

    void setallRamPix(int value);

    void on_spinBox_PTrim_2_valueChanged(int arg1);

    void on_saveRamPix_Button_clicked();

    void on_ThButton_clicked();

    std::pair<double,double> LinReg(QVector< std::pair<double, double> > points);

    void on_loadRamPix_Button_clicked();

    QVector<std::pair<unsigned int, double> > get_delay(QVector<std::pair<unsigned int, unsigned int> >  activepixels, double injection, int precision, bool newpixset);

    QVector<std::pair<unsigned int, double> > get_efficiencyV2(QVector<std::pair<unsigned int, unsigned int> >  activepixels, double injection, int precision, bool newpixset);

    void on_SaveTrim_Button_clicked();

    unsigned short gray_to_bin(unsigned short gray);

    void on_DelayCurve_button_clicked();

    void on_get_efficiency_button_clicked();

    void on_SCurveV2_Button_clicked();

    void on_SCurveallV2_Button_clicked();

    void on_SCurveAllPixAllRamval_Button_clicked();

    void on_SelectRam_Button_clicked();

    void on_get_delay_button_clicked();

    void on_Continue_Readout_clicked();

    void on_Display_Readout_Results_clicked();

    void on_liveDisplay_clicked();

    void on_Analyze_Readout_Results_clicked();

    QVector<std::array<int, 6> > LoadReadoutData();

    void on_Make_Histogram_clicked();

    void on_InjCalib_clicked();

    void on_Make_Histogram_2_clicked();

    void on_Make_Histogram_3_clicked();

    void on_Make_Histogram_4_clicked();

    void on_SCurveFit_clicked();

    QVector<double> SCurveFit(int col, int row, int RamVal);

    void on_SCurveRefit_clicked();

    void on_pushButton_clicked();

    void MoveSCurves(QString folder);

    void SaveReadoutData(QVector<std::array<int, 6> > outdata);


    void on_get_delayCurve_clicked();

    void on_Sr90Injection_clicked();

    void on_InjDelay_clicked();

    void on_get_delayCurve_2_clicked();

signals:
    void MainToHB(QVector<bool> HBConfig);
    void MainToDigSel(QVector<std::pair<unsigned int, unsigned int> >);
    void MainToGeburtstag(QVector<std::pair<unsigned int, unsigned int> >);
    void MainToReadout(std::string);
    void LiveReadout(std::pair<std::string, bool>);
    void DisplayReadoutRepaint();

public slots:
    void HBToMain(QVector<bool> HBConfig);
    void DigSelToMain(QVector<std::pair<unsigned int, unsigned int> > activepixels);
    void GeburtstagtoMain(QVector<std::pair<unsigned int, unsigned int> > activepixels);
    void DisplayReadoutClosed();


private:
    Ui::MainWindow *ui;

    FTDI* ftdi;
    GenioBase* genio;
    NexysIO* nexys;
    HBselect *HBsel;
    DigSelect *DigselUI;
    Geburtstag * GeburtstagUI;
    DisplayReadout *DisplayReadoutUI;
    QMediaPlayer *player;

    bool quiet;



};

#endif // MAINWINDOW_H
