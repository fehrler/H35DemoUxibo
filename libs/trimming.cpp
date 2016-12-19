#include "trimming.h"

Trimming::Trimming() : device(0), patterngenerator(0), chipconfig(0), cellconfig(0),
        pcbconfig(0), fastdig(0)
{

}

void Trimming::setDevice(HIF *device)
{
    if(device != 0 && *device != hifInvalid)
        this->device = device;
}

void Trimming::setPatGen(patgen *patterngenerator)
{
    if(patterngenerator != 0)
        this->patterngenerator = patterngenerator;
}

void Trimming::setChipConfig(Chip_Config *chipconfig)
{
    if(chipconfig != 0)
        this->chipconfig = chipconfig;
}

void Trimming::setCellConfig(Chip_Tune *cellconfig)
{
    if(cellconfig != 0)
        this->cellconfig = cellconfig;
}

void Trimming::setPCBConfig(PCB_Config *pcbconfig)
{
    if(pcbconfig != 0)
        this->pcbconfig = pcbconfig;
}

void Trimming::setFastReadout(fastreadout *fastdig)
{
    if(fastdig != 0)
        this->fastdig = fastdig;
}

SCurve Trimming::measureSCurve(int pixel, float Th1, float Th2,
                               double start, double end, unsigned int steps,
                               bool startsetup, bool endsetup)
{
    const unsigned int injections = 100;     //number of injections per probe point

    SCurve curve;

    //return on missing structures
    if(patterngenerator == 0 || pcbconfig == 0 || fastdig == 0)
        return curve;

    //pixel index, starting voltage or ending voltage out of range. or steps is too small
    //  for fitting (the fit function has 2 parameters)
    if(pixel >= maxpixels || pixel < 0 || start < 0 || start > pcbconfig->getVref()
            || end < 0 || end > pcbconfig->getVref() || steps < 3)
        return curve;

    //do nothing if the connection is invalid
    if(device == 0 || *device == hifInvalid)
        return curve;


    //local DAC configuration
    PCB_Config scurvepcb = *pcbconfig;
    scurvepcb.setTh1V(Th1);
    scurvepcb.setTh2V(Th2);

    //activate analog Injections:
    if(startsetup)
        Write_Reg::WriteInjectionOuts(*device, 2);

    //configure the pattern generator:
    patgen patterngenerator2 = *patterngenerator;
    if(startsetup)
    {
        patterngenerator2.setRunLength(patterngenerator->getPeriod() * injections, false);
        patterngenerator2.setReset(true, false);
    }


    float scaling = fabs(end - start) / 2;
    scurvepcb.setInjV((start+end)/2);
    unsigned int nummeasurements = 0;

    //condition to support both scan directions:
    while(nummeasurements++ < steps)
    {        
        //set starting value for Injections
        Write_Reg::WriteDAC(*device, scurvepcb.getTh1Reg(), scurvepcb.getTh2Reg(),
                            scurvepcb.getInjReg());

        //Reset fastreadout and FIFO
        fastdig->FastReset();   //sends the reset pulse at once rather than in 2 send cycles

        //start pattern generator
        patterngenerator->fastReset(false); //send the reset pulse at once to save time

        Sleep(7);  //give the FPGA time to start sending the injections

        //result data structures
        std::vector<dataset> result;
        unsigned int counter = 0;   //number of recieved hits of the investigated pixel

        //read out the registerd injections and count them:
        for(unsigned int timeout = 0; timeout < 10; timeout++)
        {
            if((fastdig->getFIFOState() & fifo_empty))  //FIFO empty
            {
                result.clear();
                Sleep(1);   //extra time for the Hardware to finish
            }
            else    //try reading all injections at once
            {
                result = fastdig->getDataSets(injections,true,false,true);

                for(std::vector<dataset>::iterator it = result.begin();
                        it != result.end(); ++it)
                {
                    //reject hits with very large delay, as they have nothing to
                    //  do with the injection
                    if(it->delay < 200 && (it->pixel1 == pixel || it->pixel2 == pixel))
                        ++counter;
                }
            }

            if(counter > injections || (counter == injections && result.size() == 0))
                break;
        }

        float efficiency = float(counter)/injections;
        curve.addPointEfficiency(scurvepcb.getInj(), efficiency);

        std::cout << "  Efficiency (" << scurvepcb.getInj() << " V): "
                  << efficiency << std::endl;

        //set injection voltage for the next measurement
        scaling /= 1.6;
        if(efficiency < 0.5)
            scurvepcb.addInjV(scaling);
        else
            scurvepcb.addInjV(-scaling);
    }


    //restore pattern generator settings from before the SCurve
    if(endsetup)
    {
        patterngenerator->setRunLength(patterngenerator->getRunLength(),false);
        patterngenerator->sendFlags();
    }
    patterngenerator->setReset(true, false);

    //restore PCB settings (injections and DACs)
    if(endsetup)
    {
        Write_Reg::WriteDAC(*device, pcbconfig->getTh1Reg(), pcbconfig->getTh2(), pcbconfig->getInjReg());
        Write_Reg::WriteInjectionOuts(*device, pcbconfig->getInjectionOuts());
    }

    return curve;
}

double Trimming::scurveFunction(double x, double x0, double width)
{
    return (1. + erf((x - x0)/width)) / 2.;
}

std::vector<SCurve> Trimming::allSCurves(unsigned char dac, float Th1, float Th2,
                                         double start, double end, unsigned int steps)
{
    std::cout << "Measure SCurves for all Pixels." << std::endl;

    Chip_Config chipcon = *chipconfig;

    chipcon.setTDAC1(0);
    chipcon.setTDAC2(0);
    chipcon.setTDAC3(dac);            //set all Th1 TuneDACs to `dac`

    for(unsigned int i = 0; i < maxpixels; ++i)
    {
        chipcon.setLoad(i,true);
        chipcon.setAnaInj(i, false);
        chipcon.setDigInj(i, false);
    }
    Write_Reg::WriteCellReg(*device,chipcon);

    for(unsigned int i = 0; i < maxpixels; ++i)
        chipcon.setLoad(i,false);

    std::vector<SCurve> curves;

    for(unsigned int i = 0; i < maxpixels; ++i)
    {
        std::cout << "Measuring Pixel " << i << std::endl;

        //activate analog Injections for the pixel to be measured
        chipcon.setAnaInj(i, true);
        Write_Reg::WriteChip(*device,chipcon);   //write the new configuration

        //perform the measurement
        PointCurve scurve = measureSCurve(i,Th1,Th2,start,end,steps,i==0,i==(maxpixels-1));
        fitSCurve(scurve);   //extract the values from the data points
        curves.push_back(scurve);

        //deactivate the current pixel
        chipcon.setAnaInj(i, false);
    }
    Write_Reg::WriteChip(*device,chipcon);

    return curves;
}

bool Trimming::saveSCurves(std::vector<SCurve>& curves, std::string filename,
                           std::string title, std::vector<unsigned int>* settings)
{
    if(curves.size() == 0)
    {
        std::cout << "No Curves in the vector." << std::endl;
        return false;
    }

    std::fstream f;
    f.open(filename.c_str(), std::ios::out | std::ios::app);

    if(!f.is_open())
        return false;

    std::vector<SCurve>::iterator it = curves.begin();
    std::vector<unsigned int>::iterator settingit;
    if(settings != 0)
     settingit = settings->begin();
    unsigned int i = 0;     //loop index for the pixel number

    while(it != curves.end())
    {
        if(settings != 0)
            f << "# " << title << " TuneDAC = " << (int)(*settingit) /*i*/ << ":" << std::endl;
        else
            f << "# " << title << " Curve # " << i << ":" << std::endl;
        for(unsigned int i=0;i<it->size();++i)
            f << (*it)[i].x << " " << (*it)[i].y << std::endl;

        f << "# Th1:               " << it->getHalfEfficiency() << std::endl
          << "# Transisiton Width: " << it->getTransitionWidth() << std::endl
          << std::endl;

        ++it;
        if(settings != 0)
            ++settingit;
        else
            ++i;
    }

    f.close();

    return true;
}

void Trimming::findmaxminthreshold(std::vector<SCurve> &curves, unsigned int &minindex,
                                   unsigned int &maxindex, double &minthr, double &maxthr)
{
    std::vector<SCurve>::iterator it = curves.begin();
    unsigned int    index = 0;
    maxthr = it->getHalfEfficiency();
    minthr = it->getHalfEfficiency();

    while(it != curves.end())
    {
        if(it->getHalfEfficiency() > maxthr)
        {
            maxindex = index;
            maxthr   = it->getHalfEfficiency();
        }
        else if(it->getHalfEfficiency() < minthr)
        {
            minindex = index;
            minthr   = it->getHalfEfficiency();
        }

        ++it;
        ++index;
    }

}

unsigned int Trimming::findThreshold1Scaling(unsigned int maxindex, double minthreshold,
                                             float Th1, float Th2,
                                             double start, double end, unsigned int steps)
{
    if(maxindex >= maxpixels)
        return 128;

    //Results of the measurements
    std::vector<SCurve> tuneScaleCurves;
    //configuration for the measurements (DAC 11 setting: VNTuneNor)
    std::vector<unsigned int> tuneScaleScale;

    Chip_Config chipcon = *chipconfig;
    unsigned int scale = 0;     //current DAC 11 setting

    //Pixel Cell Configuration:
    chipcon.setTDAC3(cellconfig->getTDAC3(maxindex));
                //set TuneDAC to "maxvalue" to find global scaling
                //sometimes, the threshold was higher with 15 than with 14 (reason unknown)
    chipcon.setAnaInj(maxindex, true);
    chipcon.setLoad(maxindex, true);
    Write_Reg::WriteCellReg(*device, chipcon); //write cell registers
    chipcon.setLoad(maxindex, false);

    //Measurements (binary search)
    for(unsigned int i = 0; i < 6; ++i)
    {
        scale |= 32 >> i;

        std::cout << "Measure Scale = " << scale << std::endl;
        //Write the new Scaling DAC value to the chip:
        chipcon.setDAC(11,scale);
        Write_Reg::WriteChip(*device, chipcon);

        //measure SCurve for new scaling and add the results to the result vectors:
        PointCurve measurement = measureSCurve(maxindex, Th1,Th2,start,end,steps,i==0,i==5);

        //check for noise in the measurement:
        bool nonoise = true;
        std::vector<datapoint>::iterator it = --measurement.getVector().end();
        while(it != measurement.getVector().begin())
        {
            if(it->y > 1.05)
                nonoise = false;
            --it;
        }

        std::cout << "Noise on this measurement: " << !nonoise << std::endl;

        if(nonoise)
            fitSCurve(measurement); //fit the last result

        tuneScaleCurves.push_back(measurement);
        tuneScaleScale.push_back(scale);


        if(!nonoise || tuneScaleCurves.back().getHalfEfficiency() < minthreshold)
            scale ^= 32 >> i;   //scale too large => clear current bit
    }

    if(scale == 0)
        //all measurements resulted in too low threshold
    {
        chipcon.setDAC(11, scale);
        Write_Reg::WriteChip(*device, chipcon);

        //measure SCurve for new scaling and add the results to the result vectors:
        PointCurve measurement = measureSCurve(maxindex, Th1,Th2,start,end,steps);
        fitSCurve(measurement);
        tuneScaleCurves.push_back(measurement);
        tuneScaleScale.push_back(scale);
        //fit the last result
    }

    //find the best setting from the measurements:
    std::vector<SCurve>::iterator curveit = tuneScaleCurves.begin();
    std::vector<unsigned int>::iterator scaleit = tuneScaleScale.begin();
    unsigned int bestscale = *scaleit;
    double difference = pcbconfig->getVref();   //maximum of possible differences
    SCurve bestcurve;

    //print out the results of the scan (also the corresponding scalings):
    std::stringstream title;
    title << "SCurve for Scaling on Pixel " << maxindex << ",";
    saveSCurves(tuneScaleCurves,"TuneScale.dat", title.str(), &tuneScaleScale);
    std::fstream f;


    //f.open("TuneScale_scalings.dat", std::ios::out | std::ios::app);

    while(curveit != tuneScaleCurves.end())
    {
        //f << *scaleit << std::endl;

        if(fabs(minthreshold - curveit->getHalfEfficiency()) < difference)
        {
            bestscale = *scaleit;
            difference = fabs(minthreshold - curveit->getHalfEfficiency());
            bestcurve = *curveit;
        }

        ++curveit;
        ++scaleit;
    }

    //f << "# best scale: " << bestscale << std::endl;
    //f.close();

    f.open("TuneScale.dat",std::ios::out | std::ios::app);
    f << "# best scale: " << bestscale << std::endl;
    f.close();

    //save the best curve also to a result file:
    std::stringstream s("");
    s << "# SCurve for Pixel " << maxindex;
    std::stringstream s2("");
    s2 << "# Th1:               " << bestcurve.getHalfEfficiency() << std::endl
       << "# Transisiton Width: " << bestcurve.getTransitionWidth();
    bestcurve.save("SCurves_trimmed.dat", s.str(),s2.str());

    //deactivate pixel `maxindex`
    chipcon.setAnaInj(maxindex, false);
    chipcon.setCompNormal(false);
    chipcon.setCompTW(false);
    chipcon.setLoad(maxindex, true);
    Write_Reg::WriteCellReg(*device, chipcon);

    return bestscale;
}

unsigned char Trimming::findTDACsetting(unsigned int pixel, double minthreshold,
                                        float Th1, float Th2,
                                        double start, double end, unsigned int steps)
{
    if(pixel >= maxpixels)
        return 128;

    std::cout << "Find TuneDAC setting for Pixel " << pixel << std::endl;

    //data containers:
    std::vector<SCurve> curves;
    std::vector<unsigned int> setting;
    unsigned char current = 0;

    //local chip configuration
    Chip_Config chipcon = *chipconfig;
    //activate pixel `pixel`:
    chipcon.setLoad(pixel,true);
    chipcon.setAnaInj(pixel, true);
    chipcon.setCompNormal(chipconfig->getCompNormal());
    chipcon.setCompTW(chipconfig->getCompTW());
        //no write operation as not necessary (done in for loop)

    //make measurements
    for(unsigned char i = 0; i < 4; ++i)
    {
        current |= 8 >> i;
        chipcon.setTDAC3(current);
        Write_Reg::WriteCellReg(*device, chipcon);

        //measure next SCurve
        PointCurve measurement = measureSCurve(pixel, Th1,Th2,start,end,steps,
                                               (pixel == 0 && i==0),(pixel == maxpixels-1 && i==3));
        fitSCurve(measurement);     //fit parameters of SCurve
        curves.push_back(measurement);
        setting.push_back(current);

        if(curves.back().getHalfEfficiency() < minthreshold)
            current ^= 8 >> i;  //deactivate bit again on too low threshold
    }

    if(current == 0)
    {
        chipcon.setTDAC3(current);
        Write_Reg::WriteCellReg(*device, chipcon);

        //measure next SCurve
        curves.push_back(measureSCurve(pixel, Th1,Th2,start,end,steps,
                                       pixel == maxpixels-1, pixel == maxpixels-1));
        setting.push_back(current);
        fitSCurve(*(--curves.end()));   //fit parameters of SCurve
    }

    //search for the best setting:
    std::vector<SCurve>::iterator        curveit   = curves.begin();
    std::vector<unsigned int>::iterator settingit = setting.begin();
    double mindiff = pcbconfig->getVref();  //maximum possible difference
    unsigned int best = *settingit;
    SCurve bestcurve;

    while(curveit != curves.end())
    {
        if(fabs(minthreshold - curveit->getHalfEfficiency()) < mindiff)
        {
            mindiff = fabs(minthreshold - curveit->getHalfEfficiency());
            best = *settingit;
            bestcurve = *curveit;
        }

        ++curveit;
        ++settingit;
    }

    //-- write out the data generated for the trimming --
    //SCurves (including the TuneDAC3 setting in the caption):
    std::stringstream str("");
    str << "TDAC_SCurves_" << pixel << ".dat";
    if(!saveSCurves(curves,str.str(), "SCurves for TuneDAC Setting,", &setting))
    {
        std::cout << "Could not Write TDAC tuning SCurves." << std::endl;
    }

    /*
    // writing out the used TuneDAC3 settings:
    std::fstream f;
    str.str("");
    str << "TDAC_Setting_" << pixel << ".dat";
    f.open(str.str(), std::ios::out | std::ios::app);
    settingit = setting.begin();
    while(settingit != setting.end())
    {
        f << *settingit << std::endl;
        ++settingit;
    }

    f.close();
    */

    //writing best SCurve to a result file:
    std::stringstream s("");
    s << "# SCurve for Pixel " << pixel;

    std::stringstream s2("");
    s2 << "# Th1:               " << bestcurve.getHalfEfficiency() << std::endl
       << "# Transisiton Width: " << bestcurve.getTransitionWidth();

    bestcurve.save("SCurves_trimmed.dat", s.str(),s2.str());

    //-- end writing data out --

    //deactivate pixel `pixel` again:
    chipcon.setAnaInj(pixel, false);
    chipcon.setCompNormal(false);
    chipcon.setCompTW(false);
    //Write_Reg::WriteCellReg(*device, chipcon);
        //changed 27.04.16: no change to tuneDACs, so a WriteCellReg() is not necessary
    Write_Reg::WriteChip(*device,chipcon);

    return best;
}


//global pointer for passing a SCurve to the fitting functions
SCurve* fitcurve;

void  function1_fvec(const alglib::real_1d_array &x, alglib::real_1d_array &fi, void *ptr)
{
    if(ptr != 0){}

    for(unsigned int i = 0; i < fitcurve->size(); ++i)
    {
        fi[i] = (Trimming::scurveFunction((*fitcurve)[i].x,x[0], x[1]) - (*fitcurve)[i].y);
               // / sqrt(fitcurve->getPoint(i).y);
    }
}
void  function1_jac(const alglib::real_1d_array &x, alglib::real_1d_array &fi,
                    alglib::real_2d_array &jac, void *ptr)
{
    if(ptr != 0){}

    for(unsigned int i = 0; i < fitcurve->size(); ++i)
    {
        fi[i] = (Trimming::scurveFunction((*fitcurve)[i].x,x[0], x[1])
                    - (*fitcurve)[i].y); // / sqrt(fitcurve->getPoint(i).y);
        jac[i][0] = -exp(pow(((*fitcurve)[i].x - x[0])/x[1],2))
                        / sqrt(/*fitcurve->getPoint(i).y * */3.1415926);
        jac[i][1] = jac[i][0] * ((*fitcurve)[i].x - x[0])/x[1];
        jac[i][0] /= x[1];
    }
}

SCurve Trimming::fitSCurve(SCurve &curve)
{
    fitcurve =&curve;   //update scurve pointer to pass the data to the fitting methods

    //set starting values for the fit
    alglib::real_1d_array x;
    x.setlength(2);
    x[0] = 1.5;
    x[1] = 0.5;

    //set termination conditions
    double epsg = 0; //1e-10;
    double epsf = 1e-10;
    double epsx = 0;
    alglib::ae_int_t maxits = 0;    //unlimited iterations

    //containers for state and result of the fit
    alglib::minlmstate state;
    alglib::minlmreport rep;

    //actual fitting:
    alglib::minlmcreatev(curve.size(), x, 1.e-4, state);    //0.0001, state);
    alglib::minlmsetcond(state, epsg, epsf, epsx, maxits);
    alglib::minlmoptimize(state, function1_fvec, function1_jac);
    alglib::minlmresults(state, x, rep);

    //write out some information about the fit:
    std::cout << "Fit Results:" << std::endl;
    std::cout << "    Iterations:   " << rep.iterationscount << std::endl;
    std::cout << "    Termination:  " << int(rep.terminationtype) << std::endl;
    std::cout << "    Parameters:   x0 = " << x[0] << ";  width = " << x[1] << std::endl;
    //std::cout << x.tostring(2) << std::endl;

    fitcurve = 0;   //unload the "passing pointer"

    //update the scurve data:
    if(rep.iterationscount > 0) //something has been done
    {
        curve.setHalfEfficiency(x[0]);
        curve.setTransitionWidth(x[1]);
    }
    else
    {
        curve.setHalfEfficiency(0);
        curve.setTransitionWidth(0);
    }

    return curve;
}

void Trimming::Th1Trimmen(float Th1, float Th2, double start, double end, unsigned int steps,
                          double ThSetPoint, bool forceTh1)
{
    if(device == 0 || *device == hifInvalid || cellconfig == 0 || chipconfig == 0
            || pcbconfig == 0 || fastdig == 0 || patterngenerator == 0)
    {
        std::cout << "Initialisation incomplete." << std::endl;
        return;
    }

    std::cout << "Trimming Th1 for all Pixels" << std::endl;

    chipconfig->setSpare(1, false); //turn on the TuneDACs
    for(unsigned int i=0; i<maxpixels;++i)
        cellconfig->setTDAC3(i,0);

    //-- Get the fist SCurves --
    std::vector<SCurve> curves = allSCurves(0, Th1, Th2, start, end, steps);

    //-- Save the SCurves --
    if(!saveSCurves(curves, "SCurves_untrimmed.dat"))
            //TODO: change to a variable name (e.g. taken from the gui)
        std::cout << "Could not write the SCurves to \"SCurves_untrimmed.dat\"" << std::endl;


    //-- Find hightest and lowest Threshold --
    unsigned int    maxindex     = 0;
    double          maxthreshold = 0;
    unsigned int    minindex     = 0;
    double          minthreshold = 0;

    findmaxminthreshold(curves, minindex, maxindex, minthreshold, maxthreshold);

    //use minimum of minthreshold and ThSetPoint as trimming goal:
    if(ThSetPoint < minthreshold || forceTh1)
    {
        minthreshold = ThSetPoint;
        minindex = maxpixels + 1;
            //this way the pixel with the smallest threshold is also trimmed
    }
    else    //lowest threshold lower than setpoint
    {
        //save the SCurve for the lowest Th1 to a result file:
        std::stringstream s("");
        s << "# SCurve for Pixel " << minindex;
        curves[minindex].save("SCurves_trimmed.dat", s.str());
    }

    //-- find scaling of the Threshold TuneDACs --
    cellconfig->setTDAC3(maxindex, 15); //14);  //changed back 26.04.16
            /* the 14 was used as sometimes the shift was smaller with 15 than with 14 */
    chipconfig->setDAC(11, findThreshold1Scaling(maxindex, minthreshold,
                                                 Th1,Th2,start,end,steps));

    //TODO: remove these lines:
    std::cout << "Min Threshold: " << minthreshold << std::endl;
    std::cout << "Max Threshold: " << maxthreshold << std::endl;
    std::cout << "Th1 TuneDAC Scaling: " << (int)chipconfig->getDAC(11) << std::endl;

    //-- find the settings for the other pixels --
    for(unsigned int i = 0; i < maxpixels; ++i)
    {
        if(i != maxindex && i != minindex)
        {
            cellconfig->setTDAC3(i, findTDACsetting(i, minthreshold, Th1,Th2,start,end,steps));

            //write setting to the chip
            chipconfig->setTDAC3(cellconfig->getTDAC3(i));
            chipconfig->setLoad(i, true);
            Write_Reg::WriteCellReg(*device, *chipconfig);
            chipconfig->setLoad(i, false);
        }
    }

            //TODO: change to a variable name (e.g. taken from the gui)
    if(!cellconfig->saveConfig("Th1TrimResults.txt"))
        std::cout << "Could not write the Trim Results to \"Th1TrimResults.txt\"" << std::endl;

    std::cout << "Trimming Done" << std::endl;
}

