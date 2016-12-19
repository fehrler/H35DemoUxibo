#ifndef POINTCURVE_H
#define POINTCURVE_H

#include <vector>
#include <algorithm>
#include <fstream>

struct datapoint {double x;
                  double y;

                 /**
                  * @brief operator <   needed to sort the vector's contents
                  */
                  bool operator < (const datapoint& pnt) const
                  {
                      return x < pnt.x;
                  }
                  datapoint operator - (const datapoint& pnt) const
                  {
                      datapoint result;
                      result.x = x - pnt.x;
                      result.y = y - pnt.y;
                      return result;
                  }
                  bool operator == (const datapoint& pnt) const
                  {
                      return (x == pnt.x) && (y == pnt.y);
                  }
                  bool operator != (const datapoint& pnt) const
                  {
                      return !((*this) == pnt);
                  }
                 };

const datapoint zero = {0,0};

/**
 * @brief The PointCurve class is a data container for measurement results and
 *          the fit parameters from fits of SCurves and exponential decays
 */
class PointCurve
{
public:
    PointCurve();

    /**
     * @brief addPoint adds a datapoint to the container
     * @param point     - the point to add
     */
    void  addPoint(datapoint point);
    /**
     * @brief addPoint adds creates a datapoint from the values given and adds it to the container
     * @param x     - x-coordinate of the datum
     * @param y     - ordinate of the datum
     */
    void  addPoint(double x, double y);
    /**
     * @brief addPointEfficiency creates and adds a datapoint to the container with
     *              regard to  constraints from the SCurve data
     * @param voltage       - voltage at which was measured
     * @param efficiency    - resulting efficiency
     */
    void  addPointEfficiency(double voltage, double efficiency);
    /**
     * @brief addPointDelay creates and adds a datapoint to the container with regard to the
     *          constraints of a delay measurement
     * @param delay         - measured delay
     * @param Th2           - Th2 setting at which was measured
     */
    void  addPointDelay(double delay, double Th2);
    /**
     * @brief delPoint erases a point from the container
     * @param index     - the index of the point to erase
     */
    void  delPoint(unsigned int index);

    /**
     * @brief getPoint returns the datapoint `index`
     * @param index     - index of the datapoint
     * @return          - the datapoint at `index`
     */
    datapoint getPoint(unsigned int index);
    /**
     * @brief getPoint returns the ordinate of the datapoint next to the passed x-coordinate
     * @param x     - the x-coordinate to search for
     * @return      - the ordinate for the datapoint next to `x`
     */
    double getPoint(double x);

    //for SCurves:
    /**
     * @brief getHalfEfficiency returns the 50% point abscissa of the fit
     * @return  - 50% point of the fit, or 0 if no fit has been performed
     */
    double getHalfEfficiency() {return halfefficiency;}
    /**
     * @brief setHalfEfficiency set the value for the 50% abscissa of the SCurve
     * @param x0
     */
    void setHalfEfficiency(double x0) { halfefficiency = x0; }
    /**
     * @brief getTransitionWidth returns the width parameter of the SCurve fit
     * @return  - the width of the transition or 0 if no fit has been performed
     */
    double getTransitionWidth() {return width;}
    /**
     * @brief setTransitionWidth set the value for the width scaling of the SCurve
     * @param width
     */
    void setTransitionWidth(double width) { this->width = width;}

    //for DischargeCurves:
    /**
     * @brief getScaling returns the scaling parameter of the exponential decay
     * @return  - the fit parameter or 0 if no fit has been performed
     */
    double getScaling() {return scaling;}
    /**
     * @brief setScaling sets the scaling parameter for the exponential decay
     * @param t0
     */
    void setScaling(double A) {this->scaling = A;}
    /**
     * @brief getTimeScale returns the time scaling of the exponential decay function
     * @return  - the time scaling or 0 if no fit has been performed
     */
    double getTimeScale() {return timescale;}
    /**
     * @brief setTimeScale set the time scaling of the exponential decay fit
     * @param timescale
     */
    void setTimeScale(double timescale);

    double getOffset() {return offset;}
    void setOffset(double offset);

    /**
     * @brief size returns the number of datapoints in the container
     * @return
     */
    unsigned int size() {return curve.size();}
    /**
     * @brief operator [] returns the datapoint at index `index` as a copy
     * @param index     - index of the datapoint to return
     * @return
     */
    datapoint operator[](int index){return curve[index];}

    std::vector<datapoint> getVector() { return curve;}

    /**
     * @brief save saves the container content to a file
     * @param filename  - name of the file to write
     * @param title     - text to write before the curve data
     * @param end       - text to write after the curve data
     * @return          - true if writing was successful of false if not written
     */
    bool save(std::string filename, std::string title = "", std::string end = "");
private:
    std::vector<datapoint> curve;
    double halfefficiency;
    double width;
    double scaling;
    double timescale;
    double offset;
};

#endif // POINTCURVE_H
