#include "pointcurve.h"

PointCurve::PointCurve()
{
    curve.clear();
    halfefficiency = 0;
    width = 0;
    scaling = 0;
    timescale = 0;
    offset = 0;
}

void PointCurve::addPoint(datapoint point)
{
    curve.push_back(point);
}

void PointCurve::addPoint(double x, double y)
{
    datapoint point = {x, y};
    curve.push_back(point);

    std::sort(curve.begin(), curve.end());
}

void PointCurve::addPointEfficiency(double voltage, double efficiency)
{
    if(efficiency < 0) // || efficiency > 1)
        return;
    addPoint(voltage, efficiency);
}

void PointCurve::addPointDelay(double Th2, double delay)
{
    if(delay < 0 || Th2 < 0)
        return;
    addPoint(Th2, delay);
}

void PointCurve::delPoint(unsigned int index)
{
    if(index < curve.size())
    {
        std::vector<datapoint>::iterator it = curve.begin();
        for(unsigned int i=0;i<index;++i)
            ++it;
        curve.erase(it);
    }
}

datapoint PointCurve::getPoint(unsigned int index)
{
    if(index < curve.size())
        return curve[index];
    else
    {
        datapoint point = {0,0};
        return point;
    }
}

double PointCurve::getPoint(double x)
{
    if(curve.size() == 0)
        return 0;

    std::vector<datapoint>::iterator bestit = curve.begin();
    double nextx = abs(bestit->x - x);

    std::vector<datapoint>::iterator it = curve.begin();

    while(it != curve.end())
    {
        if(abs(it->x - x) < nextx)
        {
            bestit = it;
            nextx = abs(it->x - x);
        }
    }

    return bestit->y;
}

void PointCurve::setTimeScale(double timescale)
{
    if(timescale > 0)
        this->timescale = timescale;
}

void PointCurve::setOffset(double offset)
{
    this->offset = offset;
}

bool PointCurve::save(std::string filename, std::string title, std::string end)
{
    if(size() == 0)
        return true;

    std::fstream f;
    f.open(filename.c_str(), std::ios::out | std::ios::app);

    if(!f.is_open())
        return false;

    f << title << std::endl;

    std::vector<datapoint>::iterator it = curve.begin();

    while(it != curve.end())
    {
        f << it->x << " " << it->y << std::endl;
        ++it;
    }
    f << end << std::endl << std::endl;

    f.close();

    return true;
}

