#include <iostream>
#include <wiringPi.h>
#include "caspar.h"
#include <vector>
#include <algorithm>
#include "GaussNewton3.h"
#include <fstream>

using namespace std;

bool runflag = false;

/* Runs a premelt to calculate the thermal fluoresence values (for our current RT implementation) and ensure good annealing of L-DNA. 
 */
void premelt()
{
    bool tempflag = false; // Indicates that the melt hasn't been finished yet.

    // Declare variables for fitting algorithm:
    double coeff[3]; // Fitting coefficients
    double iter; // Iterations required to find a fit
    double coeffprev[3]; // Previous coefficients (for convergence purposes)
    double beta0[3] = {0,0,2}; // Initial guess for the fit
    bool past_the_hump = false; // Are we past the hump of the current fit?
    bool dtrigger = false; // Flag for double hump options

    clearactivedata();     // Clear any data out of the fitting algorithm target vectors

    // Turn the heater on and the fan off, then delay to allow the data to fill again.
    digitalWrite(HEATER_PIN, HIGH);
    digitalWrite(FAN_PIN, LOW);
    delay(3000);

    while(tempflag == false && runflag == true) // While we haven't finished the melt and the user hasn't cancelled the run:
    {
        piLock(0); // Lock the thread to run a fit

        // Find the maximum element in the fitting day, and use that as our guess for midpoint and amplitude of gaussian.
        auto maxlocation = max_element(begin(derivs), end(derivs));
        beta0[0] = *maxlocation;
        beta0[1] = x[distance(derivs.begin(), maxlocation)];

        // Fit the data
        GaussNewton3(xderivs, derivs, beta0, min_vals, max_vals, coeff, &iter);

        if(coeff[1] <= x[x.size()-1]) // If we're past the hump by the fitted data, change our flag to match.
        {
            past_the_hump = true;
        }
        else
        {
            past_the_hump = false;
        }
        piUnlock(0); // Unlock the thread so it can keep taking samples.

        if (iter < 24 && abs(coeff[0]) > 5 && coeff[1] > 5)  // If we found a fit and it's not just a flat line or existing before known data:
        {
            // If we're past the hump and this fit is within a reasonable threshold of the previous one:
            if (past_the_hump == true && abs(coeffprev[0] - coeff[0]) < CONVERGENCE_THRESHOLD && abs(coeffprev[1] - coeff[1]) < CONVERGENCE_THRESHOLD && abs(abs(coeffprev[2]) - abs(coeff[2])) < CONVERGENCE_THRESHOLD)
            {
                // Accept the fit and log the coefficients.
                cout << "Coeff: " << coeff[0] << " " << coeff[1] << " " << coeff[2] << endl;
                // Keep going until we've found the endpoint of the current gaussian.
                delaytocycleend(coeff, 0.05);
                //Generate the heating curve for this gaussian.
                heatgen(coeff,dtrigger);
                if(dtrigger == false) // If we're on the first hump:
                {
                    clearactivedata(); // Clear our data.
                    dtrigger = true; // Tell the machine we're now fitting the second hump.
                }
                else
                {
                    digitalWrite(HEATER_PIN, LOW); // Turn the heater off.
                    tempflag = true; // Say that we're done generating the heat curves and doing the heat melt.
                    clearactivedata(); // Clear our data.
                }
            }

            // Move current fit to previous fit.
            coeffprev[0] = coeff[0];
            coeffprev[1] = coeff[1];
            coeffprev[2] = coeff[2];
        }
        // Wait to try fitting again.
        delay(25);
    }
}


/* Runs the RT algorithm.
 */
void runRT()
{
    int sec_hold = RT_LENGTH;
    holdtemp(60, RT_LENGTH);
    waittotemp(55);
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
        // digitalWrite(BOX_FAN, LOW),
        digitalWrite(FAN_PIN, HIGH);
        return false;
    }
    else
    {
        digitalWrite(FAN_PIN,LOW);
        readPCR();
        // digitalWrite(BOX_FAN,HIGH);
        digitalWrite(HEATER_PIN, HIGH);
        return true;
    }
}

/* Current version of the core PCR paradigm. Needs serious updates to match UI.
 */
int cycle()
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
    double dthreshcool = 0.7;
    coeffprev[0] = 0;
    coeffprev[1] = 0;
    coeffprev[2] = 0;
    int cutoff = 40;
    int triggertime;
    bool past_the_hump;
    temperrors = 0;
    delay(100);
    digitalWrite(HEATER_PIN,LOW);
    clearactivedata();
    delay(3000);

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
            if(state == true)
            {
                temperrors++;
                cout << "Heated for too long, error thrown." << endl;
                runtime_out << "Heated for too long." << endl;
                digitalWrite(HEATER_PIN,LOW);
                digitalWrite(FAN_PIN, HIGH);
                state = false;
                dtrigger = false;
                clearactivedata();
            }
            else
            {
                temperrors++;
                cout << "Cooled for too long, error thrown." << endl;
                runtime_out << "Cooled for too long." << endl;
                digitalWrite(FAN_PIN,LOW);
                digitalWrite(HEATER_PIN, HIGH);
                dtrigger = false;
                state = true;
                clearactivedata();
            }
            if(temperrors > allowed_temp_errors)
            {
                cout << "Too many heating failures--cycling ended." << endl;
                runtime_out << "Algorithm failed too many times. Cycling ended." << endl;
                runflag = false;
                digitalWrite(HEATER_PIN, LOW);
                digitalWrite(FAN_PIN, LOW);
                piUnlock(0);
                delay(100);
                sens1.stopMethod();
                sens1.LED_off(2);
                return 1;
            }
        }
        piUnlock(0);
        if (iter < 24 && abs(coeff[0]) > 10 && coeff[1] > 2.5)  
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
    runflag = false;
    digitalWrite(HEATER_PIN, LOW);
    digitalWrite(FAN_PIN, LOW);
    sens1.LED_off(2);
    return 0;
}

