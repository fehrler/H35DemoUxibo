#ifndef TRIMMING_H
#define TRIMMING_H

#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "optimization.h"
#include <chrono>
#include <ctime>
#include <sstream>

#include "mainwindow.h"
#include "chip_config.h"
#include "chip_tune.h"
#include "pcb_config.h"
#include "pointcurve.h"
#include "write_reg.h"
#include "patgen.h"
#include "fastreadout.h"

#include "dmgr.h"
#include "dpcdecl.h"
#include "depp.h"

#define maxpixels 44

typedef  PointCurve SCurve;

class Trimming
{
public:
    Trimming();

    //setter methods for config and communication structures
    void setDevice(HIF* device);
    void setPatGen(patgen* patterngenerator);
    void setChipConfig(Chip_Config* chipconfig);
    void setCellConfig(Chip_Tune* cellconfig);
    void setPCBConfig(PCB_Config* pcbconfig);
    void setFastReadout(fastreadout* fastdig);

    /**
     * @brief measureSCurve initiates the measurements for a SCurve and returns
     *                  the data
     * @param pixel             - index of the pixel to be measured
     * @param Th1               - setting of the Th1 register during measurements
     * @param Th2               - setting of the Th2 register during measurements
     * @param start             - first voltage used for measurement
     * @param end               - last voltage used for measurement
     * @param steps             - number of measurements to make
     * @param startsetup        - configure the chip before measuring
     * @param endsetup          - configure the chip after measuring (restore the old settings)
     * @return
     */
    SCurve measureSCurve(int pixel, float Th1, float Th2,
                         double start, double end, unsigned int steps,
                         bool startsetup=true, bool endsetup=true);

    /**
     * @brief fitSCurve fits a scurve to the passed data points using alglib's Levenberg-Marquard
     *              algorithm. The parameters of the fit are stored in the SCurve passed back
     * @param curve     - data points stored in an SCurve container
     * @return          - the same SCurve as passed with the resulting parameters
     */
    SCurve fitSCurve(SCurve& curve);

    /**
     * @brief Th1Trimmen finds the best TuneDAC3 values for all pixels
     * @param Th1           - Th1 value to use
     * @param Th2           - Th2 value to use
     * @param start         - start of the injection voltage interval to scan over
     * @param end           - end of the injection voltage interval to scan over
     * @param steps         - total number of measurements
     * @param ThSetPoint    - setpoint for the threshold of the pixels
     *                          (used as min(ThSetPoint, minThreshold) for the scaling)
     * @param forceTh1      - determines whether the algorithm shall decide or the setpoint
     *                          is to be take
     */
    void Th1Trimmen(float Th1, float Th2, double start, double end, unsigned int steps,
                    double ThSetPoint, bool forceTh1);

    /**
     * @brief scurveFunction evaluates the scurve for given 50% voltage and
     *              transition width
     * @param x         - voltage at which the scurve is to be evaluated
     * @param x0        - voltage, at which the scurve's value is 0.5
     * @param width     - width of the transition from 0 to 1, scaling of the erf()
     * @return          - function value of the scurve
     */
    static double scurveFunction(double x, double x0, double width);

private:
    HIF*            device;
    patgen*         patterngenerator;
    Chip_Config*    chipconfig;
    Chip_Tune*      cellconfig;
    PCB_Config*     pcbconfig;
    fastreadout*    fastdig;

    /**
     * @brief allSCurves configures the chip for a SCurve measurement for each pixel and
     *              calls the measurement method
     * @param dac       - setting of the TuneDAC3 (-> Th1) for all pixels
     * @param Th1       - setting of the Th1 register during measurements
     * @param Th2       - setting of the Th2 register during measurements
     * @param start     - start of the measurement interval (injection voltage)
     * @param end       - end of the measurement interval (injection voltage)
     * @param steps     - total number of measurements
     * @return          - a vector containing the measurement results for all pixels
     */
    std::vector<SCurve> allSCurves(unsigned char dac, float Th1, float Th2, double start, double end, unsigned int steps);

    /**
     * @brief saveSCurves saves the SCurves data passed in a file named `filename`
     * @param curves    - SCurve data to be stored in the file
     * @param filename  - name of the file to save to
     * @param title     - text to write over the data for identification
     * @param settings  - the TuneDAC settings used for the SCurves, if not passed (=0) the SCurves
     *                      will be given an index
     * @return          - true if the data was written, false if not
     */
    bool saveSCurves(std::vector<SCurve>& curves, std::string filename,
                     std::string title = "SCurve for Pixel", std::vector<unsigned int> *settings=0);

    /**
     * @brief findmaxminthreshold uses "selection sort" to find the pixels with highest and
     *              lowest threshold (50% efficiency voltage)
     * @param curves    - data base to investigate
     * @param minindex  - output variable for the index of the pixel with lowest threshold
     * @param maxindex  - output variable for the index of the pixel with highest threshold
     * @param minthr    - output variable for the lowest threshold (corresponding to `minindex`)
     * @param maxthr    - output variable for the highest threshold (corresponding to `maxindex`)
     */
    void findmaxminthreshold(std::vector<SCurve>& curves, unsigned int& minindex,
                             unsigned int& maxindex, double& minthr, double& maxthr);

    /**
     * @brief findThreshold1Scaling scans the VNTuneNor DAC (binary search) to find the
     *              best setting to eliminate the threshold difference (Th1) between the pixel
     *              cells
     * @param maxindex      - index of the pixel cell with the highest threshold
     * @param minthreshold  - lowest threshold on the chip
     * @param Th1           - setting of the Th1 register during measurements
     * @param Th2           - setting of the Th2 register during measurements
     * @param start         - start of the injection voltage interval used for the measurement
     * @param end           - end of the injection voltage interval used for the measurement
     * @param steps         - number of measurements per SCurve
     * @return              - the best setting found for DAC 11 (VNTuneNor) or 128, if the
     *                          pixel index `maxindex` is out of range
     */
    unsigned int findThreshold1Scaling(unsigned int maxindex, double minthreshold,
                                       float Th1, float Th2,
                                       double start, double end, unsigned int steps);

    /**
     * @brief findTDACsetting returns the best setting for TuneDAC3 (Th1) for pixel `pixel`
     * @param pixel         - index of the pixel to be tested
     * @param minthreshold  - lowest threshold found
     * @param Th1           - setting of the Th1 register during measurements
     * @param Th2           - setting of the Th2 register during measurements
     * @param start         - start of the injection voltage interval for measurements
     * @param end           - end of the injection voltage interval for measurements
     * @param steps         - number of measurements per SCurve
     * @return              - best setting for the TuneDAC or 128, if the index `pixel`
     *                          is out of rande
     */
    unsigned char findTDACsetting(unsigned int pixel, double minthreshold,
                                  float Th1, float Th2,
                                  double start, double end, unsigned int steps);

};

#endif // TRIMMING_H
