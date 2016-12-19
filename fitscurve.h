#ifndef FITSCURVE_H
#define FITSCURVE_H

#include <QObject>

class FitSCurve : public QObject
{
Q_OBJECT
public:
    FitSCurve();
    ~FitSCurve();
public slots:
    void SCurveFitDo();
signals:
    void SCurveFitDone();
};

#endif // FITSCURVE_H
