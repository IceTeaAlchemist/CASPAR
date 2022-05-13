#include <iostream>
#include <wiringPi.h>
#include "caspar.h"
#include <vector>
#include <algorithm>
#include "GaussNewton3.h"
#include <fstream>

using namespace std;

bool runflag = false;


/* Runs the RT algorithm and also does the premelt for heatgen. That needs to be detangled.
 */
void runRT()
{
    /*sens1.LED_off(2);
    sens1.LED_power(1,170);
    sens1.LED_on(1);*/
    bool tempflag = false;
    double temp_frac = 0.786;
    double coeff[3];
    double iter;
    double coeffprev[3];
    clearactivedata();
    delay(1000);
    int sec_hold = RT_LENGTH;
    digitalWrite(HEATER_PIN, HIGH);
    digitalWrite(FAN_PIN, LOW);
    double beta0[3] = {0,0,2};
    bool past_the_hump = false;
    bool doubleheight = false;
    bool dtrigger = false;
    while(tempflag == false)
    {
        piLock(0);
        auto maxlocation = max_element(begin(derivs), end(derivs));
        beta0[0] = *maxlocation;
        beta0[1] = x[distance(derivs.begin(), maxlocation)];
        GaussNewton3(xderivs, derivs, beta0, min_vals, max_vals, coeff, &iter);
        if(coeff[1] <= x[x.size()-1])
        {
            past_the_hump = true;
        }
        else
        {
            past_the_hump = false;
        }
        piUnlock(0);
        if (iter < 24 && abs(coeff[0]) > 10 && coeff[1] > 5)  
        {
            if (past_the_hump == true && abs(coeffprev[0] - coeff[0]) < CONVERGENCE_THRESHOLD && abs(coeffprev[1] - coeff[1]) < CONVERGENCE_THRESHOLD && abs(abs(coeffprev[2]) - abs(coeff[2])) < CONVERGENCE_THRESHOLD)
            {
                cout << "Coeff: " << coeff[0] << " " << coeff[1] << " " << coeff[2] << endl;
                /*double comparative = 1000;
                int stopindex;
                int i = 1;
                while(comparative > 1 && i < derivs.size()-2)
                {
                    if(derivs[derivs.size()-i] < comparative)
                    {
                        comparative = derivs[derivs.size()-i];
                        stopindex = derivs.size()-i + 60;
                    }
                    i++;
                }*/
                delaytocycleend(coeff, 0.005);
                heatgen(coeff, dtrigger);
                if(dtrigger == false)
                {
                    clearactivedata();
                    dtrigger = true;
                }
                else
                {
                    digitalWrite(HEATER_PIN, LOW);
                    tempflag = true;
                    clearactivedata();
                }
                /* double maxfluor = y[y.size()-1];
                double minfluor = y[stopindex];*/
            }
            coeffprev[0] = coeff[0];
            coeffprev[1] = coeff[1];
            coeffprev[2] = coeff[2];
        }
        delay(25);
    }
    RTdone = true;
    //holdtemp(65, 600);
    //holdtemp(90,60);
    /*sens1.LED_off(1);
    sens1.LED_power(1,45);*/
}

// Delays to a point based on the fitted coefficients and the value of the derivative, depending on a fraction of peak threshold. Returns the time in milliseconds after program start.
int delaytocycleend(const double coeff[3], double thresh)
{
    while(abs(derivs[derivs.size()-1]) > abs(coeff[0] * thresh))
    {
        delay(10);
    }
    cout << "Absolute value of " << derivs[derivs.size()-1] << " is less than " << thresh*coeff[0] << endl;
    return data[data.size()-1].timestamp - data[0].timestamp;
}

// Shifts the shift between heating and cooling. Takes the current mode as an argument, returns the new one.
bool modeshift(bool state)
{
    if(state == true)
    {
        digitalWrite(HEATER_PIN, LOW);
        digitalWrite(FAN_PIN, HIGH);
        return false;
    }
    else
    { 
        digitalWrite(FAN_PIN,LOW);
        readPCR();
        digitalWrite(HEATER_PIN, HIGH);
        return true;
    }
}

/* Current version of the core PCR paradigm. Needs serious updates to match UI.
 */
void cycle()
{
    int mode = 2;
    bool doublehump;
    if(mode == 2)
    {
        doublehump = true;
    }
    else
    {
        doublehump = false;
    }
    bool dtrigger = false;
    double beta0[3] = {10, 10, 1};
    double lb[3] = {min_vals[0], min_vals[1], min_vals[2]};
    double ub[3] = {max_vals[0], max_vals[1], max_vals[2]};
    double coeff[3];
    double iter;
    double coeffprev[3];
    double coeffdouble[3];
    double thresh = 0.005;
    double threshcool = 0.135;
    double dthreshheat = 0.1;
    double dthreshcool = 0.5;
    coeffprev[0] = 0;
    coeffprev[1] = 0;
    coeffprev[2] = 0;
    int cutoff = 40;
    int triggertime;
    bool past_the_hump;
    delay(100);
    // cout << "Attempting to run RT step: " << endl;
    digitalWrite(HEATER_PIN,LOW);
    clearactivedata();
    delay(1000);

    // Begin heating.
    digitalWrite(HEATER_PIN, HIGH);
    digitalWrite(FAN_PIN,LOW);   
    bool state = true;
    delay(100);
    while (cycles < cutoff && runflag == true)
    {
        piLock(0);
        if(state == true)
        {
            auto maxlocation = max_element(begin(derivs), end(derivs));
            beta0[0] = *maxlocation;
            beta0[1] = x[distance(derivs.begin(), maxlocation)];
        }
        else
        {
            auto minlocation = min_element(begin(derivs), end(derivs));
            beta0[0] = *minlocation;
            beta0[1] = x[distance(derivs.begin(), minlocation)];
        }
        GaussNewton3(xderivs, derivs, beta0, lb, ub, coeff, &iter);
        if(coeff[1] < x[x.size()-1])
        {
            past_the_hump = true;
        }
        else
        {
            past_the_hump = false;
        }
        if(x[x.size()-1] > 75)
        {
            cout << "Cycled for too long, error thrown." << endl;
            digitalWrite(FAN_PIN,LOW);
            digitalWrite(HEATER_PIN,LOW);
            runflag = false;
        }
        piUnlock(0);
        if (iter < 24 && abs(coeff[0]) > 10 && coeff[1] > 1)  
        {
            if (past_the_hump == true && abs(coeffprev[0] - coeff[0]) < CONVERGENCE_THRESHOLD && abs(coeffprev[1] - coeff[1]) < CONVERGENCE_THRESHOLD && abs(abs(coeffprev[2]) - abs(coeff[2])) < CONVERGENCE_THRESHOLD)
            {
                cout << coeff[0] << "   " << coeff[1] << "  " << coeff[2]  << "   " << "  Iter: " << iter << endl;
                if(doublehump == false)
                {
                    if(state == true)
                    {
                        triggertime = delaytocycleend(coeff,thresh);
                    }
                    else
                    {
                        triggertime = delaytocycleend(coeff,threshcool);
                    }
                    cout << "Control triggered at " << triggertime << " milliseconds after run initiation." << endl;
                    coeff_out << triggertime << "," << coeff[0] << "," << coeff[1] << "," << coeff[2] << "," << triggertime - run_start << endl; 
                    if(state == false)
                    {
                        pcr_out << triggertime << ",";
                    }
                    state = modeshift(state);
                    clearactivedata();
                    if(state == true)
                    {
                        //lb[0] = -max_vals[0];
                        //ub[0] = -min_vals[0];
                    }
                    else
                    {
                        //lb[0] = min_vals[0];
                        //ub[0] = max_vals[0];
                        cout << "Cycle " << cycles << " complete." << endl;
                        cycles++;
                    }
                }
                else
                {
                    if(dtrigger == false)
                    {
                        dtrigger = true;
                        if(state == true)
                        {
                            triggertime = delaytocycleend(coeff,dthreshheat);
                        }
                        else
                        {
                            triggertime = delaytocycleend(coeff,dthreshcool);
                        }
                    }
                    else
                    {
                        if(state == true)
                        {
                            triggertime = delaytocycleend(coeff,thresh);
                            //lb[0] = -max_vals[0];
                            //ub[0] = -min_vals[0];
                        }
                        else
                        {
                            triggertime = delaytocycleend(coeff,threshcool);
                            //lb[0] = min_vals[0];
                            //ub[0] = max_vals[0];
                            pcr_out << triggertime << ",";
                            cycles++;
                        }
                        state = modeshift(state);
                        cout << "Cycle " << cycles << " complete." << endl;
                        dtrigger = false;
                    }
                    cout << "Control triggered at " << triggertime << " milliseconds after run initiation." << endl;
                    coeff_out << triggertime << "," << coeff[0] << "," << coeff[1] << "," << coeff[2] << "," << triggertime - run_start << endl; 
                    clearactivedata();
                }
            }
            coeffdouble[0] = coeffprev[0];
            coeffdouble[1] = coeffprev[1];
            coeffdouble[2] = coeffprev[2];
            coeffprev[0] = coeff[0];
            coeffprev[1] = coeff[1];
            coeffprev[2] = coeff[2];
        }
        delay(100);
    }
    // fclose(output);
    digitalWrite(HEATER_PIN, LOW);
    digitalWrite(FAN_PIN, LOW);
    sens1.LED_off(2);
}

