#include <iostream>
#include <wiringPi.h>
#include "caspar.h"
#include <vector>
#include <algorithm>
#include "GaussNewton3.h"
#include "GaussNewton4.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <wiringPiI2C.h>

using namespace std;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

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
    setDACvoltage(1.5);
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
            if (past_the_hump == true && abs(coeffprev[0] - coeff[0]) < CONVERGENCE_THRESHOLD && 
                abs(coeffprev[1] - coeff[1]) < CONVERGENCE_THRESHOLD && 
                abs(abs(coeffprev[2]) - abs(coeff[2])) < CONVERGENCE_THRESHOLD)
            {
                // Accept the fit and log the coefficients.
                cout << "premelt: Coeffs: " << coeff[0] << " " << coeff[1] << " " << coeff[2] << endl;
                // Keep going until we've found the endpoint of the current gaussian.
                //delaytocycleend(coeff, 0.05, AMPL_MIN_PREMELT);  // Hardcoded heating threshold number!!
                //Generate the heating curve for this gaussian.
                heatgen(coeff,dtrigger);
                RTfluor = heatquery(coeff, 0.5);
                
                if(dtrigger == false) // If we're on the first hump:
                {
                    clearactivedata(); // Clear our data.
                    dtrigger = true; // Tell the machine we're now fitting the second hump.
                    tempflag = true; // Nick edit: Prevent overheat. Remove after swine testing.
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
    holdfluor(RTfluor,RT_LENGTH);
    setDACvoltage(0.0);
    digitalWrite(HEATER_PIN,LOW);
    int falliterator = 0;
    if(runflag == true)
    {
        digitalWrite(FAN_PIN,HIGH);
    }
    while(runflag == true, falliterator < 500)
    {
        delay(10);
        falliterator++;
    }
    digitalWrite(FAN_PIN, LOW);
    // waittotemp(RT_WAITTOTEMP);
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

// Hmmm, with four param fit and effort to change AMPL_MIN_HTP or LTP to signed, negative
// for negative derivs, and positive for postitive derivs.

// Heating OR Cooling could be negative going derivatives, so check C0 > C3 POSitive deriv,
// and C0 < C3 NEGative derivs.
long delaytocycleend(const double coeff[4], double thresh, double ampl_min)
{
    // int isPos = (coeff[0]-ampl_min)>0? 1: -1; // Was (coeff[0]-coeff[3]) but coeff[3] can be big when wrong(?).
    // derivs[] is updated in other thread while this loop is going.
    while( ampl_min*( derivs[derivs.size()-1] - ((coeff[0]-coeff[3])*thresh + coeff[3]) ) > 0 )
    {
        delay(10);
    }

    // while(abs(derivs[derivs.size()-1]) > abs(coeff[0] * thresh)+coeff[3])
    // {
    //     delay(10);
    // }
    // cout << "delaytocycleend: Absolute value of derivs[last] " << derivs[derivs.size()-1] << endl;
    // cout << "\t is less than thresh*coeff[0] + coeff[3] " << thresh*coeff[0] << " plus " << coeff[3] << endl;
    cout << "delaytocycleend: ampl_min " << ampl_min << " derivs[last] " << derivs[derivs.size()-1];
    cout << " and test condition \"below\" " << endl;
    cout << "\t (coeff[0]-coeff[3])*thresh + coeff[3] " << ((coeff[0]-coeff[3])*thresh + coeff[3]) << endl;
   
    // return data[data.size()-1].timestamp; Altered for melt... may cause issues.
    return x[x.size()-1];
}

// Shifts the shift between heating and cooling. Takes the current mode as an argument, 
// returns the new one.  state = false is cooling, true is heating.
bool modeshift(bool state)
{
    if(state == true)  // Had been heating.
    {
        if(cool_power > 0 && !overdriveflag)
        {
            setDACvoltage(cool_power);
        }
        else
        {
            digitalWrite(HEATER_PIN, LOW);
            setDACvoltage(0.0);
        }
        if(active_cooling || overdriveflag)
        {
            digitalWrite(FAN_PIN, HIGH);
        }
        if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
        phaseend = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        phasestart = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
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
        else // HTP[0] == 2, other Qiagen, only methods 1 and 3 are valid in our code.
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
        if(meltflag)
        {
            changeQiagen(MELTP);
        }
        else
        {
            changeQiagen(LTP);
        }
        piUnlock(0);
        if(!meltflag)
        {
            delay(500);
        }
        return false;  // Return false, cooling state.
    }
    else  // state = false, the cooling state.
    {
        digitalWrite(FAN_PIN,LOW);
        phaseend = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        if(HOLD_ON)
        {
            holdfluor(y[y.size()-1], HOLD_LENGTH);
        }
        setDACvoltage(0.0);
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
        if(meltflag)
        {
            changeQiagen(MELTP);
        }
        else
        {
            changeQiagen(HTP);
        }
        piUnlock(0);
        if (pwm_enable) pwmWrite(PWM_PIN, pwm_high);
        digitalWrite(HEATER_PIN, HIGH); //Moved 1/24/25 for dual control with laser. Move before delay for fastest cycling.
        setDACvoltage(power_setting);
        phasestart = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        if(!meltflag)
        {
            delay(500);
        }
        // Turn the cycling LED back on.
        recordflag = true;
        // digitalWrite(BOX_FAN,HIGH);
        return true;
    }
}

// Shifts the shift between heating and cooling. Takes the current mode as an argument, 
// returns the new one.  state = false is cooling, true is heating.
bool meltshift(bool state)
{
    if(state == true)  // Had been heating.
    {
        digitalWrite(HEATER_PIN, LOW);
        // digitalWrite(FAN_PIN, HIGH);
        setDACvoltage(cool_power);
        if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
        phaseend = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        phasestart = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        delay(500);
        return false;  // Return false, cooling state.
    }
    else  // state = false, the cooling state.
    {
        digitalWrite(FAN_PIN,LOW);
        phaseend = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        recordflag = false;
      
        piLock(0);
        sens1->LED_off(1);
        sens2->LED_off(1);
        sens1->LED_off(2);
        sens2->LED_off(2);
        sens1->stopMethod();
        sens2->stopMethod();
        piUnlock(0);
        return true;
    }
}

/* Current version of the core PCR paradigm. Needs serious updates to match UI. ??weg
 */
int cycle()
{
    string progName = "cycle";
    ostringstream bstream;
    digitalWrite(HEATER_PIN,LOW);
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
    power_setting = LASER_POWER;
    //int mode = 2;  // Double hump.  See below for check with single_hump boolean.
    bool doublehump = !single_hump;  // Calc from recipe, if HTP==LTP, single_hump is false.
    // doublehump = false; //fix this
    bool dtrigger = false;
    overdriveflag = false;
    bool inversioncool = true;
    double inversioncooltarget;
    bool inversiontrigger = false;
    int fittingvars = 4;
    double beta0[fittingvars] = {10, 10, 1, 0};
    double lb[fittingvars] = {min_vals[0], min_vals[1], min_vals[2], 0};
    double ub[fittingvars] = {max_vals[0], max_vals[1], max_vals[2], 0};
    double coeff[fittingvars];
    double iter;
    double coeffprev[fittingvars];
    double coeffdouble[fittingvars];
    // double thresh = 0.05;  // Looked at LV and recipe for CDZ double hump has 0.05 here, 20221026 weg.
    // double threshcool = 0.135;
    // double dthreshheat = 0.25; // Other version has 0.25, 20220915 weg.
    // double dthreshcool = 0.8; // Other version has 0.8, ditto weg.
    double thresh = THRESH;
    double threshcool = THRESHCOOL;
    double dthreshheat = DTHRESHHEAT;
    double dthreshcool = DTHRESHCOOL;
    double AMPL_MIN, CTR_MIN;
    bool CHECKFIT;  // The list of conditions to call it a good fit.
    double heattoolongActual; // For taking the value of heattoolongfirst (cycles 0) or heattoolong.
    error anerror;  // For filling the errorArray.
    coeffprev[0] = 0;
    coeffprev[1] = 0;
    coeffprev[2] = 0;
    coeffprev[3] = 0;
    int cutoff = cyclecutoff;
    int fittingcutoff = cyclesfit; //Bill, this hardcoded value can be replaced to fix the code.
    int usedtimes = cyclesaverage; //same here.
    vector<int> heating_times;
    vector<int> cooling_times;
    vector<double> high_fluors;
    vector<double> low_fluors;
    double triggerfluor;
    cout << progName << ": cutoff (and cyclecutoff) are " << cutoff << " ." << endl;
    long triggertime; // 20220822 weg, from int to long
    bool past_the_hump;
    meltflag = false;
    temperrors = 0;
    delay(100);
    digitalWrite(HEATER_PIN, HIGH); //BILL LOOKIE
    cout << "DAC Check #4 " << DACHandle << endl;
    if(DACHandle == -1)
    {
        DACHandle = wiringPiI2CSetup(0x60);
        if(DACHandle < 0)
        {
            cout << "DAC Communication failed. " << endl;
        }
        else
        {
            cout << "Communicating with DAC with handle " << DACHandle << endl;
        }
    }
    // int DACHandleLocal = wiringPiI2CSetup(0x60);
    setDACvoltage(power_setting);

    phasestart = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    double savestart;
    digitalWrite(FAN_PIN,LOW);
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_high);
    delay(500);
    clearactivedata();
    delay(1000);// Tried 100ms and it for sure causes a segmentation violation AFTER
    // the heater starts!!  Mess with this at your peril.  20221028 weg

    // Begin heating.
     
    bool state = true;  // Set the heating flag.
    delay(100);
    // Update the cutoff at the end of while loop or just use cyclecutoff and not cutoff?? (weg)

    while (cycles < fittingcutoff && cycles < cutoff && runflag == true && cycles < overdrive_start)
    {
        piLock(0);
        if(state == true)  // The heating cycle.
        {
            auto maxlocation = max_element(begin(derivs), end(derivs));
            beta0[0] = *maxlocation;
            beta0[1] = xderivs[distance(derivs.begin(), maxlocation)];
        }
        else   // The cooling cycle.
        {
            auto minlocation = min_element(begin(derivs), end(derivs));
            beta0[0] = *minlocation;
            beta0[1] = xderivs[distance(derivs.begin(), minlocation)];
        }
        GaussNewton4(xderivs, derivs, beta0, coeff, &iter);
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
        // Separately for cooling and heating, heattoolongfirst, heattoolong, and cooltoolong in caspar.h and in the
        // recipes file.  Counter cycles = 0 in casparapi and setup.
        if ( state == true )  // The heating cycle.
        {
            if (cycles == 0) heattoolongActual = heattoolongfirst;
            else heattoolongActual = heattoolong;

            if(x[x.size()-1] > heattoolongActual)
            {
                temperrors++;
                bstream.str("");
                bstream << progName << ": Heated for too long, error thrown, " << heattoolongActual << " secs";
                bstream << " on cycles " << cycles << " ." << endl;
                cout << bstream.str();
                runtime_out << bstream.str();

                anerror.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                anerror.progname = progName;
                anerror.message = bstream.str();
                errorArray.push_back(anerror);

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
                bstream.str(""); 
                bstream << progName << ": Cooled for too long, error thrown, " << cooltoolong << " secs.";
                bstream << " on cycles " << cycles << " ." << endl;
                cout << bstream.str();
                runtime_out << bstream.str();

                anerror.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                anerror.progname = progName;
                anerror.message = bstream.str();
                errorArray.push_back(anerror);       

                state = modeshift(state);  // modeshift has piLock() in it!  Make sure it is unlocked when this is executed.
                dtrigger = false;
                clearactivedata();
                // Should there be some printouts here, like above, cycle end, and a pcr_out?? weg 20230911
                cycles++;  // If in these errors there is NO gaussian fit, so below will not trigger the cycles incrementing.
            }
        }

        if(temperrors > allowed_temp_errors)
        {
            bstream.str(""); 
            bstream << progName << ": Too many heating/cooling failures, " << allowed_temp_errors << " , cycling ended." << endl;
            cout << bstream.str();
            runtime_out << bstream.str();

            anerror.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            anerror.progname = progName;
            anerror.message = bstream.str();
            errorArray.push_back(anerror);

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
        
        // Bypass the heating control algorithm if inversion cooling is being used.
        if(inversioncool == true && state == false && y[y.size()-1] < inversioncooltarget && doublehump == true)
        {
            inversiontrigger = true;
        }
        else
        {
            inversiontrigger = false;
        }


        //  if (iter < 24 && abs(coeff[0]) > 10 && coeff[1] > 1) // Other version, 20220915 weg.
        //  Check that if there is a gaussian fit, typ AMPL_MIN is 10.0 and CTR_MIN 2.0.
        //  The thresholds are THRESH = 0.05, THRESHCOOL = 0.135, DTHRESHHEAT = 0.25, and DTHRESHCOOL = 0.8,
        //  and CONVERGENCE_THRESHOLD = 1 .
        if (state==true) // In heating cycle.
        {
            AMPL_MIN = AMPL_MIN_HTP;  // Heating may not always be a positive bump!!  Sign handled by AMPL_MIN*() below.
            CTR_MIN = CTR_MIN_HTP;
        } else  // In cooling cycle.
        {
            AMPL_MIN = AMPL_MIN_LTP;  // Should be negative if Fluor starts high and is going low. weg 20240208
            CTR_MIN = CTR_MIN_LTP;           
        }
        if ( (iter < ITER_MAX && AMPL_MIN*(coeff[0]-AMPL_MIN) > 0 && coeff[1] > CTR_MIN)  || inversiontrigger) // Can have negative sigmas! 
        // Successfully found a fit for the deriv of fluorescence.
        {
            if ((past_the_hump == true && abs(coeffprev[0] - coeff[0]) < CONVERGENCE_THRESHOLD && 
                    abs(coeffprev[1] - coeff[1]) < CONVERGENCE_THRESHOLD && 
                    abs(abs(coeffprev[2]) - abs(coeff[2])) < CONVERGENCE_THRESHOLD &&
                    abs(abs(coeffprev[3] - coeff[3])) < CONVERGENCE_THRESHOLD) || inversiontrigger)
            {
                cout << progName << ": coeffs[], Amplitude " << coeff[0] << " Center " << coeff[1] << " Width ";
                cout <<  coeff[2]  << endl << "\t Offset " << coeff[3] << "  Iteration " << iter << endl;
                if(doublehump == false)
                {
                    if(state == true)  // heating
                    {
                        triggertime = delaytocycleend(coeff,thresh,AMPL_MIN);
                        triggerfluor = y[y.size()-1];
                        high_fluors.push_back(triggerfluor);
                    }
                    else  // cooling
                    {
                        triggertime = delaytocycleend(coeff,threshcool,AMPL_MIN);
                        triggerfluor = y[y.size()-1];
                        low_fluors.push_back(triggerfluor);
                    }
                    cout << progName << ": Control triggered at " << triggertime - (long)run_start;
                    cout << " milliseconds after initiation." << endl;
                    cout << "With fluorophore reading of " << triggerfluor << "." << endl;
                    
                    coeff_out << triggertime << "," << coeff[0] << "," << coeff[1] << "," << coeff[2] << "," << coeff[3];
                    coeff_out << "," << triggertime - (long)run_start << endl; 
                    if(state == false)  // cooling
                    {
                        cout << progName << ": Cycle " << cycles << " complete." << endl;
                        cycles++;
                    }
                    savestart = phasestart;
                    state = modeshift(state);
                    clearactivedata();
                    if(state == false)//These are inverted due to the modeshift function swapping state before we get here.
                    {
                        heating_times.push_back(phaseend-savestart);
                        cout << "Phase time: " << heating_times[heating_times.size()-1] << endl;
                    }
                    else
                    {
                        cooling_times.push_back(phaseend-savestart);
                        cout << "Phase time: " << cooling_times[cooling_times.size()-1] << endl;
                    }
                }
                else // doublehump is true
                {
                    if(dtrigger == false && inversiontrigger == false)  // Just found the first hump and inversion hasn't occurred yet.
                    {
                        dtrigger = true;
                        if(state == true)  // heating
                        {
                            triggertime = delaytocycleend(coeff,dthreshheat,AMPL_MIN);
                            inversioncooltarget = calculateInversion(coeff[1],coeff[2]);
                            cout << "Inversion cooling target fluor is: " << inversioncooltarget << endl;
                            setDACvoltage(overdrive_power);
                        }
                        else   // cooling
                        {
                            triggertime = delaytocycleend(coeff,dthreshcool,AMPL_MIN);
                        }
                    }
                    else if(dtrigger == true || inversiontrigger == true) // dtrigger is true, looking for the second/double hump.
                    {
                        if(state == true)   // heating
                        {
                            setDACvoltage(power_setting);
                            triggertime = delaytocycleend(coeff,thresh,AMPL_MIN);
                            triggerfluor = y[y.size()-1];
                            high_fluors.push_back(triggerfluor);
                            //lb[0] = -max_vals[0];
                            //ub[0] = -min_vals[0];
                        }
                        else if(state == false && inversiontrigger == true)
                        {
                            triggertime = x[x.size()-1];
                            triggerfluor = y[y.size()-1];
                            low_fluors.push_back(triggerfluor);
                            //lb[0] = min_vals[0];
                            //ub[0] = max_vals[0];
                            cycles++;
                            cout << "\n\nINVERSION TRIGGERED." << endl << endl;
                            inversiontrigger = false;
                        }
                        else  // cooling
                        {
                            triggertime = delaytocycleend(coeff,threshcool,AMPL_MIN);
                            triggerfluor = y[y.size()-1];
                            low_fluors.push_back(triggerfluor);
                            //lb[0] = min_vals[0];
                            //ub[0] = max_vals[0];
                            cycles++;
                            inversiontrigger = false;
                        }
                        cout << "Double hump control: Trigger fluor is " << triggerfluor << endl;
                        savestart = phasestart;
                        state = modeshift(state);
                        if(state == false)//These are inverted due to the modeshift function swapping state before we get here.
                        {
                            heating_times.push_back(phaseend-savestart);
                            cout << "Phase time: " << heating_times[heating_times.size()-1] << endl;
                        }
                        else
                        {
                            cooling_times.push_back(phaseend-savestart);
                            cout << "Phase time: " << cooling_times[cooling_times.size()-1] << endl;
                        }
                        cout << progName << ": Cycle " << cycles << " complete." << endl;
                        dtrigger = false;
                    }
                    cout << progName << ": Control triggered at " << triggertime - (long)run_start;
                    cout << " milliseconds after initiation." << endl;

                    coeff_out << triggertime << "," << coeff[0] << "," << coeff[1] << "," << coeff[2] << "," << coeff[3];
                    coeff_out << "," << triggertime - (long)run_start << endl;                  
                    clearactivedata();
                }   // end doublehump is true
            }
            coeffdouble[0] = coeffprev[0];
            coeffdouble[1] = coeffprev[1];
            coeffdouble[2] = coeffprev[2];
            coeffdouble[3] = coeffprev[3];
            coeffprev[0] = coeff[0];
            coeffprev[1] = coeff[1];
            coeffprev[2] = coeff[2];
            coeffprev[3] = coeff[3];
        }// end if there is a gaussian fit.
        delay(100);
        // Update the cyclecutoff in case it was updated.
        cutoff = cyclecutoff;
/*        if(x[x.size()-1] > 12) //This is a debug loop, if you need to see what it's trying when it's failing to find a fit.
        {
            cout << "Fitting algorithm over warning limit." << endl;
            cout << "Initial Guess: Amplitude: " << beta0[0] << " Center: " << beta0[1] << endl;
            cout << "Coefficients tried: ";
            cout << progName << ": coeffs[], Amplitude " << coeff[0] << " Center " << coeff[1] << " Width ";
            cout <<  coeff[2]  << endl << "\t Offset " << coeff[3] << "  Iteration " << iter << endl;
        }*/
    }   // end while running cycles, while (cycles < cutoff && runflag == true)  
    // fclose(output);
    
    // Overdrive protocol, for running slow and then fast. 
    if(cycles < fittingcutoff && cycles < cutoff && cycles >= overdrive_start && runflag == true)
    {
        overdriveflag = true;
        double heat_target = 0;
        int used_values = 0;
        double cool_target = 0;
        power_setting = overdrive_power;
        setDACvoltage(power_setting);
        for(int i = 0; i < high_fluors.size(); i++)
        {
            heat_target += high_fluors[i];
            cool_target += low_fluors[i];
            used_values++;
        }
        heat_target = heat_target/used_values;
        cool_target = cool_target/used_values;
        cout << "Initiating overdrive." <<endl;
        cout << "Cycles used: " << used_values << endl;
        cout << "Heat target: " << heat_target << "mV." << endl;
        cout << "Cooling time: " << cool_target << "mV." << endl;
        int delay_adjustment = 0;
        while (cycles < fittingcutoff && runflag == true)
        {
            while(y[y.size()-1] < heat_target - 10 && runflag == true)
            {
                usleep(50000);
            }
            savestart = phasestart;
            state = modeshift(state);
            heating_times.push_back(phaseend-savestart);
            cout << "Phase time: " << heating_times[heating_times.size()-1] << endl;
            while(y[y.size()-1] > cool_target && runflag == true)
            {
                usleep(50000);
            }
            cycles++;
            savestart = phasestart;
            if(cycles >= fittingcutoff && cycles < cutoff)
            {
                meltflag = autopilotmelt;
            }
            if(runflag == true)
            {
                state = modeshift(state);
            }
            cooling_times.push_back(phaseend-savestart);
            cout << "Phase time: " << cooling_times[cooling_times.size()-1] << endl;
            cout << "Overdrive: Cycle " << cycles << " complete." << endl;
        }
    }
    
    if(cycles >= fittingcutoff && cycles < cutoff && runflag == true)
    {
        meltflag = autopilotmelt;
        int recentcycle = heating_times.size()-1;
        double heat_time = 0;
        double cool_time = 0;
        for(int i = 0; i < usedtimes; i++)
        {
            heat_time += heating_times[recentcycle - i];
            cool_time += cooling_times[recentcycle - i];
        }
        heat_time = heat_time/usedtimes;
        cool_time = cool_time/usedtimes;
        cout << "Autopilot calibrated." <<endl;
        cout << "Cycles used: " << usedtimes << endl;
        cout << "Heating time: " << heat_time << "ms." << endl;
        cout << "Cooling time: " << cool_time << "ms." << endl;
        int delay_adjustment = 0;
        int i = 0;
        while (cycles < cutoff && runflag == true)
        {
            i = 0;
            heat_time = heat_time;
            while(i < 100 && runflag == true)
            {
                usleep((heat_time-delay_adjustment)*10);
                i++;
            }
            i = 0;
            state = modeshift(state);
            while(i < 100 && runflag == true)
            {
                usleep((cool_time-delay_adjustment)*10);
                i++;
            }
            cycles++;
            if(runflag == true)
            {
                state = modeshift(state);
            }
            cout << "Autopilot: cycle " << cycles << " complete." << endl;
        }
    }
    runflag = false;
    digitalWrite(HEATER_PIN, LOW);
    cout << progName << ":  pwm_enable is " << pwm_enable << endl;
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
    digitalWrite(FAN_PIN, LOW);
    sens1->LED_off(2);
    sens1->LED_off(1);
    sens2->LED_off(2);
    sens2->LED_off(1);
    setDACvoltage(0.0);
    return 0;
}// end cycle

/* Melt protocol
 */
int melt()
{
    string progName = "melt";
    ostringstream bstream;
    digitalWrite(HEATER_PIN,LOW);
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
    //int mode = 2;  // Double hump.  See below for check with single_hump boolean.
    bool doublehump = true;  // Needs both peaks as a comparator.
    // doublehump = false; //fix this
    bool dtrigger = false;
    int fittingvars = 4;
    double beta0[fittingvars] = {10, 10, 1, 0};
    double lb[fittingvars] = {min_vals[0], min_vals[1], min_vals[2], 0};
    double ub[fittingvars] = {max_vals[0], max_vals[1], max_vals[2], 0};
    double coeff[fittingvars];
    double iter;
    double coeffprev[fittingvars];
    double coeffdouble[fittingvars];
    // double thresh = 0.05;  // Looked at LV and recipe for CDZ double hump has 0.05 here, 20221026 weg.
    // double threshcool = 0.135;
    // double dthreshheat = 0.25; // Other version has 0.25, 20220915 weg.
    // double dthreshcool = 0.8; // Other version has 0.8, ditto weg.
    double thresh = THRESH;
    double threshcool = THRESHCOOL;
    double dthreshheat = DTHRESHHEAT;
    double dthreshcool = DTHRESHCOOL;
    double AMPL_MIN, CTR_MIN;
    bool CHECKFIT;  // The list of conditions to call it a good fit.
    double heattoolongActual; // For taking the value of heattoolongfirst (cycles 0) or heattoolong.
    error anerror;  // For filling the errorArray.
    coeffprev[0] = 0;
    coeffprev[1] = 0;
    coeffprev[2] = 0;
    coeffprev[3] = 0;
    vector<int> heating_times;
    vector<int> cooling_times;
    long triggertime; // 20220822 weg, from int to long
    bool past_the_hump;
    temperrors = 0;
    delay(100);
    digitalWrite(HEATER_PIN, HIGH); //BILL LOOKIE
    setDACvoltage(LASER_POWER);
    phasestart = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    double savestart;
    digitalWrite(FAN_PIN,LOW);
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_high);
    delay(500);
    clearactivedata();
    delay(1000);// Tried 100ms and it for sure causes a segmentation violation AFTER
    // the heater starts!!  Mess with this at your peril.  20221028 weg

    // Begin heating.
     
    bool state = true;  // Set the heating flag.
    delay(100);
    int meltcycle = 0;
    while (runflag == true && meltcycle == 0)
    {
        piLock(0);
        if(state == true)  // The heating cycle.
        {
            auto maxlocation = max_element(begin(derivs), end(derivs));
            beta0[0] = *maxlocation;
            beta0[1] = xderivs[distance(derivs.begin(), maxlocation)];
        }
        else   // The cooling cycle.
        {
            auto minlocation = min_element(begin(derivs), end(derivs));
            beta0[0] = *minlocation;
            beta0[1] = xderivs[distance(derivs.begin(), minlocation)];
        }
        GaussNewton4(xderivs, derivs, beta0, coeff, &iter);
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
        // Separately for cooling and heating, heattoolongfirst, heattoolong, and cooltoolong in caspar.h and in the
        // recipes file.  Counter cycles = 0 in casparapi and setup.
        if ( state == true )  // The heating cycle.
        {
            if (cycles == 0) heattoolongActual = heattoolongfirst;
            else heattoolongActual = heattoolong;

            if(x[x.size()-1] > heattoolongActual)
            {
                temperrors++;
                bstream.str("");
                bstream << progName << ": Heated for too long, error thrown, " << heattoolongActual << " secs";
                bstream << " on cycles " << cycles << " ." << endl;
                cout << bstream.str();
                runtime_out << bstream.str();

                anerror.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                anerror.progname = progName;
                anerror.message = bstream.str();
                errorArray.push_back(anerror);

                state = meltshift(state);
                dtrigger = false;
                clearactivedata();
            }

        }
        else  // The cooling cycle.
        {
            if(x[x.size()-1] > cooltoolong)// Check for first or second hump, add in the time from the first hump.
            {
                temperrors++;
                bstream.str(""); 
                bstream << progName << ": Cooled for too long, error thrown, " << cooltoolong << " secs.";
                bstream << " on cycles " << cycles << " ." << endl;
                cout << bstream.str();
                runtime_out << bstream.str();

                anerror.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                anerror.progname = progName;
                anerror.message = bstream.str();
                errorArray.push_back(anerror);       

                state = meltshift(state);  // modeshift has piLock() in it!  Make sure it is unlocked when this is executed.
                dtrigger = false;
                clearactivedata();
                // Should there be some printouts here, like above, cycle end, and a pcr_out?? weg 20230911
                cycles++;  // If in these errors there is NO gaussian fit, so below will not trigger the cycles incrementing.
            }
        }


        //  if (iter < 24 && abs(coeff[0]) > 10 && coeff[1] > 1) // Other version, 20220915 weg.
        //  Check that if there is a gaussian fit, typ AMPL_MIN is 10.0 and CTR_MIN 2.0.
        //  The thresholds are THRESH = 0.05, THRESHCOOL = 0.135, DTHRESHHEAT = 0.25, and DTHRESHCOOL = 0.8,
        //  and CONVERGENCE_THRESHOLD = 1 .
        if (state==true) // In heating cycle.
        {
            AMPL_MIN = AMPL_MIN_HTP;  // Heating may not always be a positive bump!!  Sign handled by AMPL_MIN*() below.
            CTR_MIN = CTR_MIN_HTP;
        } else  // In cooling cycle.
        {
            AMPL_MIN = AMPL_MIN_LTP;  // Should be negative if Fluor starts high and is going low. weg 20240208
            CTR_MIN = CTR_MIN_LTP;           
        }
        if ( (iter < ITER_MAX && AMPL_MIN*(coeff[0]-AMPL_MIN) > 0 && coeff[1] > CTR_MIN )  ) // Can have negative sigmas! 
        // Successfully found a fit for the deriv of fluorescence.
        {
            if (past_the_hump == true && abs(coeffprev[0] - coeff[0]) < CONVERGENCE_THRESHOLD && 
                    abs(coeffprev[1] - coeff[1]) < CONVERGENCE_THRESHOLD && 
                    abs(abs(coeffprev[2]) - abs(coeff[2])) < CONVERGENCE_THRESHOLD &&
                    abs(abs(coeffprev[3] - coeff[3])) < CONVERGENCE_THRESHOLD)
            {
                cout << progName << ": coeffs[], Amplitude " << coeff[0] << " Center " << coeff[1] << " Width ";
                cout <<  coeff[2]  << endl << "\t Offset " << coeff[3] << "  Iteration " << iter << endl;
                if(doublehump == false)
                {
                    if(state == true)  // heating
                    {
                        triggertime = delaytocycleend(coeff,thresh,AMPL_MIN);
                    }
                    else  // cooling
                    {
                        triggertime = delaytocycleend(coeff,threshcool,AMPL_MIN);
                    }
                    cout << progName << ": Control triggered at " << triggertime - (long)run_start;
                    cout << " milliseconds after initiation." << endl;
                    
                    if(state == false)  // cooling
                    {
                        cout << progName << ": Cycle " << cycles << " complete." << endl;
                        cycles++;
                    }
                    savestart = phasestart;
                    state = meltshift(state);
                    clearactivedata();
                    if(state == false)//These are inverted due to the modeshift function swapping state before we get here.
                    {
                        heating_times.push_back(phaseend-savestart);
                        cout << "Phase time: " << heating_times[heating_times.size()-1] << endl;
                    }
                    else
                    {
                        cooling_times.push_back(phaseend-savestart);
                        cout << "Phase time: " << cooling_times[cooling_times.size()-1] << endl;
                    }
                }
                else // doublehump is true
                {
                    if(dtrigger == false)  // Just found the first hump. 
                    {
                        dtrigger = true;
                        if(state == true)  // heating
                        {
                            triggertime = delaytocycleend(coeff,dthreshheat,AMPL_MIN);
                        }
                        else   // cooling
                        {
                            triggertime = delaytocycleend(coeff,dthreshcool,AMPL_MIN);
                        }
                    }
                    else  // dtrigger is true, looking for the second/double hump.
                    {
                        cout << "State: " << state << endl;
                        if(state == true)   // heating
                        {
                            triggertime = delaytocycleend(coeff,0.01,AMPL_MIN);
                            //lb[0] = -max_vals[0];
                            //ub[0] = -min_vals[0];
                        }
                        else  // cooling
                        {
                            triggertime = delaytocycleend(coeff,threshcool,0.01);
                            //lb[0] = min_vals[0];
                            //ub[0] = max_vals[0];
                            meltcycle++;
                            cout << progName << "Melt complete. Check file for data." << endl;
                            runflag = false;
                            cycles++;
                        }
                        savestart = phasestart;
                        state = meltshift(state);
                        if(state == false)//These are inverted due to the modeshift function swapping state before we get here.
                        {
                            heating_times.push_back(phaseend-savestart);
                            cout << "Phase time: " << heating_times[heating_times.size()-1] << endl;
                        }
                        else
                        {
                            cooling_times.push_back(phaseend-savestart);
                            cout << "Phase time: " << cooling_times[cooling_times.size()-1] << endl;
                        }
                        dtrigger = false;
                    }
                    cout << progName << ": Control triggered at " << triggertime - (long)run_start;
                    cout << " milliseconds after initiation." << endl;
               
                    clearactivedata();
                }   // end doublehump is true
            }
            coeffdouble[0] = coeffprev[0];
            coeffdouble[1] = coeffprev[1];
            coeffdouble[2] = coeffprev[2];
            coeffdouble[3] = coeffprev[3];
            coeffprev[0] = coeff[0];
            coeffprev[1] = coeff[1];
            coeffprev[2] = coeff[2];
            coeffprev[3] = coeff[3];
        }// end if there is a gaussian fit.
        delay(100);
    } 
    // fclose(output);
    cout << progName << ":  pwm_enable is " << pwm_enable << endl;
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
    state = true;
    digitalWrite(FAN_PIN, LOW);
    runflag = false;
    digitalWrite(HEATER_PIN, LOW);
    cout << progName << ":  pwm_enable is " << pwm_enable << endl;
    if (pwm_enable) pwmWrite(PWM_PIN, pwm_low);
    digitalWrite(FAN_PIN, LOW);
    sens1->LED_off(2);
    setDACvoltage(0.0);
    return 0;
}// end melt

// changeQiagen - Switch to another Qiagen and Method.  The
// qiagenproperties is {Qiagen, Method} and is usually called HTP or LTP.
void changeQiagen(vector<int> qiagenproperties)
{
    string progName = "changeQiagen";
    int LED;
    // if(qiagenproperties[1] == 3)
    // {
    //     LED = 2;
    // }
    // else
    // {
    //     LED = 1;
    // }
    LED = ( qiagenproperties[1] == 3? LED = 2: LED = 1);

    sens1->stopMethod();
    sens2->stopMethod();
    fittingqiagen = qiagenproperties[0];
    if(qiagenproperties[0] == 1)
    {
        sens1->setMethod(qiagenproperties[1]);
        sens1->writeqiagen(0, {255,255});
        sens1->startMethod();
        sens1->LED_on(LED);
        if(qiagenproperties[1] == 4)
        {
            sens1->LED_on(2);
        }
    }
    else
    {
        sens2->setMethod(qiagenproperties[1]);
        sens2->writeqiagen(0, {255,255});
        sens2->startMethod();
        sens2->LED_on(LED);
        if(qiagenproperties[1] == 4)
        {
            sens2->LED_on(2);
        }
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
    cout << "\tFitting Qiagen is " << fittingqiagen << " with Method " << qiagenproperties[1] << "." << endl;
}

int setDACvoltage(float voltage)
{
	int voltstoset = voltage/5.0*4096;
	if(voltstoset > 1638 || voltstoset < 0)
	{
		cout << "setDACvoltage: Exceeding laser safety parameters. Exiting without setting power." << endl;
		return 1;
	}
	int commandbyte;
	switch(DACChannel) {
		case 0:
			commandbyte = 0b01011000;
			break;
		case 1:
			commandbyte = 0b01011010;
			break;
		case 2:
			commandbyte = 0b01011100;
			break;
		case 3:
			commandbyte = 0b01011110;
			break;
		default:
			cout << "Invalid channel selected. Range is 0-3." << endl;
			return 2;
	}
 //   cout << "Setting Voltage with Parameters:" << endl;
 //   cout << "Handle: " << DACHandle << "    Command Byte: " << commandbyte << "    Voltage: " << voltage << endl;
 //   cout << "Binary representation: " << voltstoset << endl;
    cout << "Setting voltage: " << voltage << endl;
	wiringPiI2CWriteReg16(DACHandle,commandbyte,__bswap_16(voltstoset));
	return 0;
}

double calculateInversion(double midpoint, double width)
{
    int i = 0;
    bool notfound = true;
    while(i < x.size() && notfound)
    {
        if(x[i] > midpoint - width*1.35)
        {
            notfound = false;
        }
        i++;
    }
    return y[i];
}
