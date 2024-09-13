#include "caspar.h"
#include <iostream>
#include <deque>
#include <vector>
#include <wiringPi.h>
#include <chrono>
#include <ctime>
#include "ADC.h"
#include "qiagen.h"
#include <fstream>
#include <string>

using namespace std;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

bool pcrReady = false;
vector<double> pcrValues;

/* Averages a deque, specifically of doubles. This doesn't exist at base for some reason but we need it for the moving average filter.
 * 
 * Note that errors can result if the deque is altered while averaging is occurring.
 *
 * Args:
 *  queue: deque to be averaged.
 * 
 * Returns:
 *  sum: average of the deque.
 */
double mean(const deque<double> queue)
{
    double sum;
    for(int i = 0; i < queue.size(); i++)
    {
        sum += queue[i];
    }
    sum = sum/queue.size();
    return sum;
}

/* Empties the vectors that the control algorithm targets. Should be called whenever a clean slate is desired. 
 */
void clearactivedata()
{
    x.clear();
    y.clear();
    xderivs.clear();
    derivs.clear();
    yaverage.clear();
}


/* Is called whenever the D2 ADC (generic ADC0?) has a sample ready and performs all relevant updates.
 * This includes:
 *  Matching the ADC value to a time.
 *  Storing both.
 *  Performing the moving average filter.
 *  Writing the raw data to file.
 */
void sampletriggered(void)
{
    if(recordflag == true) // If we're supposed to be recording right now. This exists to allow this thread to be turned off.
    {
    // Locking the thread here prevents it from interfering with global variables while they're being used by other portions of 
    // the program that lock the same thread.
    piLock(0);
    // Get the current time in milliseconds.
    auto millisecs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    // Declare a reading object and put our timestamp and fluoresence inside, then push it into our data vector.
    reading now;
    now.timestamp = millisecs;
    now.voltage = ADC0->getreading();
    data.push_back(now);
    // If it's our first entry this fitting, set the starting time for our fit to our current reading.
    if(x.size() == 0)
    {
        run_start = now.timestamp;
    }

    // Push our start-adjusted x into our value queue.
    x.push_back((now.timestamp - run_start)/1000.0);
    // Get the current fluoresence reading from the struct and convert it to mV.
    double ycurrent =(now.voltage/32768.0)*4096.0; // FIX THIS!!!  HARDCODED.
    if(yaverage.size() >= SMOOTHING) // If our moving average filter is full:
    { 
        yaverage.pop_front(); // Remove the oldest value.
    }
    yaverage.push_back(ycurrent); // Push the new value onto the averaging stack.
    double average = mean(yaverage); // Average the filter values.
    y.push_back(average); // Push the average into the y stack.
    if(y.size() > SMOOTHING) // If our values are no longer being affected by 0--i.e., the moving average filter is full:
    {
        xderivs.push_back((now.timestamp - run_start)/1000.0); // Push a time value for this derivative onto the xderiv vector.
        double deriv = (y[y.size() - 1] - y[y.size() - 2])*25; // Calculate the derivative and push it onto the stack. (y1-y2)/(delta T).
        derivs.push_back(deriv);
    }
    // Create a pointer to our reading.
    reading *ptr = &now;
    // Write the data into our binary file.
    fwrite(ptr, sizeof(reading), 1, output);
    piUnlock(0);
    }
    else
    {
        //If we aren't supposed to be recording, do nothing.
    }
}

// retrieveSample - Retrieves the Qiagen data.
// Called from casparapi.cpp in the thread mechanism.
void retrieveSample()
{
    if(recordflag == true)
    {
    // Locking the thread here prevents it from interfering with global variables while they're being used by other portions of the 
    // program that lock the same thread.
    piLock(0);
    // Get the current time in milliseconds.
    auto millisecs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    // Declare a reading object and put our timestamp and fluoresence inside, then push it into our data vector.
    reading now;
    now.timestamp = millisecs;

    if(fittingqiagen == 1)
    {
        now.voltage = sens1->measure();
    }
    else
    {
        now.voltage = sens2->measure();
    }
    data.push_back(now);
    // If it's our first entry this fitting, set the starting time for our fit to our current reading.
    if(x.size() == 0)
    {
        run_start = now.timestamp;
    }

    // Push our start-adjusted x into our value queue.
    x.push_back((now.timestamp - run_start)/1000.0);
    // Get the current fluoresence reading from the struct and convert it to mV.
    double ycurrent = now.voltage;
    if(yaverage.size() >= SMOOTHING) // If our moving average filter is full:
    { 
        yaverage.pop_front(); // Remove the oldest value.
    }
    yaverage.push_back(ycurrent); // Push the new value onto the averaging stack.
    double average = mean(yaverage); // Average the filter values.
    y.push_back(average); // Push the average into the y stack.
    reading currentderiv;
    currentderiv.timestamp = now.timestamp;
    if(y.size() > SMOOTHING) // If our values are no longer being affected by 0--i.e., the moving average filter is full:
    {
        xderivs.push_back((now.timestamp - run_start)/1000.0); // Push a time value for this derivative onto the xderiv vector.
        // double deriv = (y[y.size() - 1] - y[y.size() - 2])*10; // Calculate the derivative and push it onto the stack. (y1-y2)/(delta T). Single stencil. Replaced with 5 7/14/22 NAS
        double deriv = (-y[y.size() - 1] + 8*y[y.size() - 2] - 8*y[y.size() - 4] + y[y.size()-5])/(0.1*12.0);
        derivs.push_back(deriv);
        currentderiv.voltage = deriv;
    }
    else
    {
        currentderiv.voltage = 0.0;
    }
    // Create a pointer to our reading.
    reading *ptr = &now;
    // Write the data into our binary file.
    fwrite(ptr, sizeof(reading), 1, output);
    reading *ptr2 = &currentderiv;
    fwrite(ptr2,sizeof(reading), 1, output);
    piUnlock(0);
    }
    else  // recordflag is false.
    {
        // do nothing
    }
}

/* Returns the current time as a string of the format YYYYMMDD_HHMMSS.
*/
string timestamp()
{
    time_t now = time(0); // Get the time.
    tm* ltm = localtime(&now); // Set a pointer to the LOCAL time.

    // Retrieve each member and put them into our string.
    string currenttime = to_string(1900 + ltm->tm_year); 
    int ival = 1+ltm->tm_mon;
    currenttime += ( ival < 10 ? string(1,'0').append( to_string(ival) ) : to_string(ival) );
    //currenttime += to_string(1 + ltm->tm_mon);  // Make sure this month is 01, 02, ..., 09, 10, 11, 12 .  Leading zero.
    ival = ltm->tm_mday;
    currenttime += ( ival < 10 ? string(1,'0').append( to_string(ival) ) : to_string(ival) );
    //currenttime += to_string(ltm->tm_mday);
    currenttime += "_";
    ival = ltm->tm_hour;
    currenttime += ( ival < 10 ? string(1,'0').append( to_string(ival) ) : to_string(ival) );
    //currenttime += to_string(ltm->tm_hour);
    ival = ltm->tm_min;
    currenttime += ( ival < 10 ? string(1,'0').append( to_string(ival) ) : to_string(ival) );
    //currenttime += to_string(ltm->tm_min);
    ival = ltm->tm_sec;
    currenttime += ( ival < 10 ? string(1,'0').append( to_string(ival) ) : to_string(ival) );
    //currenttime += to_string(ltm->tm_sec);

    // Return the string.
    return currenttime;
}

/* Reads all 3 PCR values and writes them to file. This will need to be updated to also feed data to the UI and be impacted by recipe. 
 */
void readPCR()
{
    auto millisecs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    pcr_out << millisecs << ",";
    pcrValues.push_back(cycles);
    pcrValues.push_back(millisecs);
    double PCRread;
    if(channelflags[0] == 1) //If channel is ON.
    {
        sens2->LED_on(1);
        sens2->setMethod(1);
        sens2->startMethod();
        delay(500);
        PCRread = sens2->measure();
        sens2->stopMethod();
        sens2->LED_off(1); 
    }
    else
    {
        PCRread = 0;
    }
    pcr_out << PCRread << ",";
    pcrValues.push_back(PCRread);

    if(channelflags[1] == 1)
    {
        sens2->LED_on(2);
        sens2->setMethod(3);
        sens2->startMethod();
        delay(500);
        PCRread = sens2->measure();
        sens2->stopMethod();
        sens2->LED_off(2); 
    }
    else
    {
        PCRread = 0;
    }
    pcr_out << PCRread  << ",";
    pcrValues.push_back(PCRread);

    // Take our first detector value. Each of these follows the same format, so I'll only comment the first.
    if(channelflags[2] == 1)
    {
        sens1->LED_on(1); // Turn the relevant LED on.
        sens1->setMethod(1); // Set our method to the relevant one.
        sens1->startMethod(); // Start our LED sampling.
        delay(500); // Wait for a sample to be taken--this needs to be OVER 333 ms at least.
        PCRread = sens1->measure(); //Get the measurement from the qiagen.
        sens1->stopMethod(); // Stop sampling.
        sens1->LED_off(1); // Turn the LED off.
    }
    else
    {
        PCRread = 0;
    }
    pcr_out << PCRread << ","; // Dump the PCR value to the file and follow it with a comma for csv processing.
    pcrValues.push_back(PCRread); // Push the PCR value onto the result array.
    
    
    // Take our first detector value. Each of these follows the same format, so I'll only comment the first.
    if(channelflags[3] == 1)
    {
        sens1->LED_on(2); // Turn the relevant LED on.
        sens1->setMethod(3); // Set our method to the relevant one.
        sens1->startMethod(); // Start our LED sampling.
        delay(500); // Wait for a sample to be taken--this needs to be OVER 333 ms at least.
        PCRread = sens1->measure(); //Get the measurement from the qiagen.
        sens1->stopMethod(); // Stop sampling.
        sens1->LED_off(2); // Turn the LED off.
    }
    else
    {
        PCRread = 0;
    }
    pcr_out << PCRread << ","; // Dump the PCR value to the file and follow it with a comma for csv processing.
    pcrValues.push_back(PCRread); // Push the PCR value onto the result array.

    pcr_out << endl; // Send a new line to the file to get ready for the next cycle.
    pcrReady = true; // Say that our PCR readings are ready for output to the gui.
}

// retrieveTemperatures - Retrieves the Temperature ADC data.
// Also the ADC0 data, for slow reads.  See Nick's original code/thread for fast Qiagen reads.
// Called from casparapi.cpp in the thread mechanism.
void retrieveTemperatures()
{
    string progName = "retrieveTemperatures";

    // cout << progName << ": recordflag is " << recordflag << endl;
    if(recordflag == true)
    {
    // Locking the thread here prevents it from interfering with global variables while they're being 
    // used by other portions of the program that lock the same thread.
    piLock(1);  // Use 1 as the temperatures key.
    // Declare a reading object and put our timestamp and fluoresence inside, then push it into our 
    // data vector.
    reading now0_tempers, now1_tempers, now0_adc0;   // timestamp and voltage for reading
    
    // Reading two temperatures, ends up at slightly different times.  
    // Configure a0-a3 on asd115.
    TEMP->SetMultiplex(0,3);
    usleep(40000);  // 40ms wait seemed to work in other codes doing this switch channels.  WEG
    now0_tempers.voltage = TEMP->convertToDegC( TEMP->getvoltage() );
    // Get the current time in milliseconds.
    now0_tempers.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    
    TEMP->SetMultiplex(1,3);
    usleep(40000);
    now1_tempers.voltage = TEMP->convertToDegC( TEMP->getvoltage() );   
    now1_tempers.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    ADC0->SetMultiplex(0,3);
    usleep(40000);
    now0_adc0.voltage = ( ADC0->getvoltage() );   
    now0_adc0.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    data0_tempers.push_back(now0_tempers);
    data1_tempers.push_back(now1_tempers);
    data0_adc0.push_back(now0_adc0);

    // The run_start has been reset when x.size() == 0.  Just use it here?

    x0_tempers.push_back( now0_tempers.timestamp - run_start );
    x1_tempers.push_back( now1_tempers.timestamp - run_start );
    x0_adc0.push_back( now0_adc0.timestamp - run_start );
    y0_tempers.push_back( now0_tempers.voltage );
    y1_tempers.push_back( now1_tempers.voltage );
    y0_adc0.push_back( now0_adc0.voltage );

    temper_out << now0_tempers.timestamp <<"\t" << now0_tempers.voltage << "\t";
    temper_out << now1_tempers.timestamp <<"\t" << now1_tempers.voltage << "\t";
    temper_out << now0_adc0.timestamp <<"\t" << now0_adc0.voltage << endl;

    // cout << progName << ": now0 " << now0_tempers.timestamp <<"\t" << now0_tempers.voltage << "\t";
    // cout << "\t now1 " << now1_tempers.timestamp <<"\t" << now1_tempers.voltage << endl;

    piUnlock(1);
    }
    else  // recordflag is false.
    {
        // do nothing
    }




}// end retrieveTemperatures


