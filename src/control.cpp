#include <iostream>
#include <wiringPi.h>
#include "caspar.h"
#include <vector>
#include <algorithm>
#include "GaussNewton3.h"
#include <fstream>
#include <sstream>

using namespace std;

bool runflag = false;
// Cannot call devicesIni.get_value() here, gives sementation fault.  Is an extern in caspar.h .  20221117 weg
// double heattoolong = stod( devicesIni.get_value("MISC", "heattoolong") );  // secs, 75 typically, too long? 20221027 weg
// double cooltoolong = stod( devicesIni.get_value("MISC", "cooltoolong") );  // secs, 75 typically
// double heattoolong = 75;
// double cooltoolong = 75;
// For above see L75 in setup.cpp .  The above are called BEFORE the devicesINI is instantiated...betchya.

/* Runs a premelt to calculate the thermal fluoresence values (for our current RT implementation) 
   and ensure good annealing of L-DNA. 
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
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_high);
    delay(3000);

    // While we haven't finished the melt and the user hasn't cancelled the run:
    while(tempflag == false && runflag == true) 
    {
        piLock(0); // Lock the thread to run a fit.

        // Find the maximum element in the fitting day, and use that as our guess for midpoint 
        // and amplitude of gaussian.
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

        // If we found a fit and it's not just a flat line or existing before known data:
        // Was 24, 5, 5, weg 20221111.
        if (iter < ITER_MAX_PREMELT && abs(coeff[0]) > AMPL_MIN_PREMELT && coeff[1] > CTR_MIN_PREMELT)  
        {
            // If we're past the hump and this fit is within a reasonable threshold of the previous one:
            if (past_the_hump == true && abs(coeffprev[0] - coeff[0]) < CONVERGENCE_THRESHOLD && abs(coeffprev[1] - coeff[1]) < CONVERGENCE_THRESHOLD && abs(abs(coeffprev[2]) - abs(coeff[2])) < CONVERGENCE_THRESHOLD)
            {
                // Accept the fit and log the coefficients.
                cout << "premelt: Coeffs: " << coeff[0] << " " << coeff[1] << " " << coeff[2] << endl;
                // Keep going until we've found the endpoint of the current gaussian.
                delaytocycleend(coeff, 0.05);  // Hardcoded heating threshold number!!
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
                    if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
                    tempflag = true; // Say that we're done generating the heat curves 
                    // and doing the heat melt.
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
}//end premelt


/* Runs the RT algorithm.
 */
void runRT()
{
    holdtemp(RT_TEMP, RT_LENGTH);
    waittotemp(RT_WAITTOTEMP);
}

/* Runs the RT algorithm.
 */
void reconstitute()
{
    holdtemp(RECON_TEMP, RECON_LENGTH);
    waittotemp(RECON_WAITTOTEMP);
}

// Delays to a point based on the fitted coefficients and the value of the derivative, depending 
// on a fraction of peak threshold. Returns the time from epoch when control triggered.
// Changed to long from int, weg 20220822.
long delaytocycleend(const double coeff[3], double thresh)
{
    while(abs(derivs[derivs.size()-1]) > abs(coeff[0] * thresh))
    {
        delay(10);
    }
    cout << "delaytocycleend: Absolute value of derivs[last] " << derivs[derivs.size()-1] << " is less than thresh*coeff[0] " << thresh*coeff[0] << endl;
    return data[data.size()-1].timestamp;
}

// Shifts the shift between heating and cooling. Takes the current mode as an argument, 
// returns the new one.  state = false is cooling, true is heating.
bool modeshift(bool state)
{
    if(state == true)  // Had been heating.
    {
        digitalWrite(HEATER_PIN, LOW);
        digitalWrite(FAN_PIN, HIGH);
        if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
        piLock(0);
        if(HTP[0] == 1)
        {
            if(HTP[1] == 3)
            {
                sens1->LED_off(2);
            }
            else
            {
                sens1->LED_off(1);
            }
        }
        else
        {
            if(HTP[1] == 3)
            {
                sens2->LED_off(2);
            }
            else
            {
                sens2->LED_off(1);
            }
        }
        changeQiagen(LTP);
        piUnlock(0);
        delay(500);
        return false;  // Return false, cooling state.
    }
    else  // state = false, the cooling state.
    {
        digitalWrite(FAN_PIN,LOW);
        recordflag = false;
      
        piLock(0);

        if(LTP[1] == 1)
        {
            if(LTP[2] == 3)
            {
                sens1->LED_off(2);
            }
            else
            {
                sens1->LED_off(1);
            }
        }
        else
        {
            if(LTP[2] == 3)
            {
                sens2->LED_off(2);
            }
            else
            {
                sens2->LED_off(1);
            }
        }
        sens1->LED_off(1);
        sens2->LED_off(1);
        sens1->LED_off(2);
        sens2->LED_off(2);
        sens1->stopMethod();
        sens2->stopMethod();
        readPCR();
        changeQiagen(HTP);
        piUnlock(0);
        delay(500);
        // Turn the cycling LED back on.
        recordflag = true;
        // digitalWrite(BOX_FAN,HIGH);
        digitalWrite(HEATER_PIN, HIGH);
        if (pwm_enable) pwmWrite(PWM_PIN, pwm_high);
        return true;
    }
}

/* Current version of the core PCR paradigm. Needs serious updates to match UI.
 */
int cycle()
{
    string progName = "cycle";
    ostringstream bstream;
    int mode = 2;  // Hard coded double hump.  See Nick edits.
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
    // double thresh = 0.05;  // Looked at LV and recipe for CDZ double hump has 0.05 here, 20221026 weg.
    // double threshcool = 0.135;
    // double dthreshheat = 0.25; // Other version has 0.25, 20220915 weg.
    // double dthreshcool = 0.8; // Other version has 0.8, ditto weg.
    double thresh = THRESH;
    double threshcool = THRESHCOOL;
    double dthreshheat = DTHRESHHEAT;
    double dthreshcool = DTHRESHCOOL;
    coeffprev[0] = 0;
    coeffprev[1] = 0;
    coeffprev[2] = 0;
    int cutoff = cyclecutoff;
    cout << "control.cpp: cycle(): cutoff (and cyclecutoff) are " << cutoff << " ." << endl;
    long triggertime; // 20220822 weg, from int to long
    bool past_the_hump;
    temperrors = 0;
    delay(100);
    digitalWrite(HEATER_PIN,LOW);
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
    clearactivedata();
    delay(3000);// Tried 100ms and it for sure causes a segmentation violation AFTER
    // the heater starts!!  Mess with this at your peril.  20221028 weg

    // Begin heating.
    digitalWrite(HEATER_PIN, HIGH);
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_high);
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
        piUnlock(0);

        // Check the heat or cool too long.  How to handle the clearing of data after first hump---save it somewhere.
        // Separately for cooling and heating, heattoolong and cooltoolong in caspar.h .
        if ( state == true )  // The heating cycle.
        {
            if(x[x.size()-1] > heattoolong)// Check for first or second hump, add in the time from the first hump.
            {
                temperrors++;
                bstream << progName << ": Heated for too long, error thrown, " << heattoolong << " secs." << endl;
                cout << bstream.str();
                runtime_out << bstream.str();

                state = modeshift(state);
                dtrigger = false;
                clearactivedata();
            }

        }
        else  // The cooling cycle.
        {
            if(x[x.size()-1] > cooltoolong)// Check for first or second hump, add in the time from the first hump.
            {
                temperrors++;
                bstream << progName << ": Cooled for too long, error thrown, " << cooltoolong << " secs." << endl;
                cout << bstream.str();
                runtime_out << bstream.str();

                state = modeshift(state);  // modeshift has piLock() in it!  Make sure it is unlocked when this is executed.
                dtrigger = false;
                clearactivedata();
            }
        }

        if(temperrors > allowed_temp_errors)
        {
            bstream << progName << ": Too many heating/cooling failures, " << allowed_temp_errors << " , cycling ended." << endl;
            cout << bstream.str();
            runtime_out << bstream.str();
            runflag = false;
            digitalWrite(HEATER_PIN, LOW);
            digitalWrite(FAN_PIN, LOW);
            if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
            piUnlock(0);
            delay(100);
            sens1->stopMethod();
            sens1->LED_off(2);
            return 1;
        }


//        if (iter < 24 && abs(coeff[0]) > 10 && coeff[1] > 1) // Other version, 20220915 weg.
        if (iter < ITER_MAX && abs(coeff[0]) > AMPL_MIN && coeff[1] > CTR_MIN)  
        {
            if (past_the_hump == true && abs(coeffprev[0] - coeff[0]) < CONVERGENCE_THRESHOLD && abs(coeffprev[1] - coeff[1]) < CONVERGENCE_THRESHOLD && abs(abs(coeffprev[2]) - abs(coeff[2])) < CONVERGENCE_THRESHOLD)
            {
                cout << progName << ": coeffs[0] etc, Amplitude " << coeff[0] << " Center " << coeff[1] << " Width " << coeff[2]  << 
                "  Iteration " << iter << endl;
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
                    cout << progName << ": Control triggered at " << triggertime - (long)run_start << " milliseconds after initiation." << endl;
                    coeff_out << triggertime << "," << coeff[0] << "," << coeff[1] << "," << coeff[2] << "," << triggertime - (long)run_start << endl; 
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
                        cout << progName << ": Cycle " << cycles << " complete." << endl;
                        cycles++;
                    }
                }
                else // doublehump is true
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
                        cout << progName << ": Cycle " << cycles << " complete." << endl;
                        dtrigger = false;
                    }
                    cout << progName << ": Control triggered at " << triggertime - (long)run_start << " milliseconds after initiation." << endl;
                    coeff_out << triggertime << "," << coeff[0] << "," << coeff[1] << "," << coeff[2] << "," << triggertime - (long)run_start << endl; 
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
    cout << progName << ":  pwm_enable is " << pwm_enable << endl;
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
    digitalWrite(FAN_PIN, LOW);
    sens1->LED_off(2);
    return 0;
}// end cycle

void changeQiagen(vector<int> qiagenproperties)
{
    string progName = "changeQiagen";
    int LED;
    if(qiagenproperties[1] == 3)
    {
        LED = 2;
    }
    else
    {
        LED = 1;
    }
    sens1->stopMethod();
    sens2->stopMethod();
    fittingqiagen = qiagenproperties[0];
    if(qiagenproperties[0] == 1)
    {
        sens1->setMethod(qiagenproperties[1]);
        sens1->writeqiagen(0, {255,255});
        sens1->startMethod();
        sens1->LED_on(LED);
    }
    else
    {
        sens2->setMethod(qiagenproperties[1]);
        sens2->writeqiagen(0, {255,255});
        sens2->startMethod();
        sens2->LED_on(LED);
    }
    cout << progName << ": Shifting to method " << qiagenproperties[1] << " on Qiagen " << qiagenproperties[0] << "." << endl;
    cout << "\tBoard is ";
    if (qiagenproperties[0] == 1)
    {   
        cout << sens1->getBoardID() << endl;
    } else
    {
        cout << sens2->getBoardID() << endl;
    }
    cout << "\tFitting qiagen is " << fittingqiagen << "." << endl;
}