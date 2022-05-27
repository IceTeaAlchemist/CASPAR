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
}


/* Is called whenever the D2 ADC has a sample ready and performs all relevant updates.
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
    // Locking the thread here prevents it from interfering with global variables while they're being used by other portions of the program that lock the same thread.
    piLock(0);
    // Get the current time in milliseconds.
    auto millisecs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    // Declare a reading object and put our timestamp and fluoresence inside, then push it into our data vector.
    reading now;
    now.timestamp = millisecs;
    now.voltage = D2.getreading();
    data.push_back(now);
    // If it's our first entry this fitting, set the starting time for our fit to our current reading.
    if(x.size() == 0)
    {
        run_start = now.timestamp;
    }

    // Push our start-adjusted x into our value queue.
    x.push_back((now.timestamp - run_start)/1000.0);
    // Get the current fluoresence reading from the struct and convert it to mV.
    double ycurrent =(now.voltage/32768.0)*4096.0;
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

/* Returns the current time as a string of the format YYYYMMDD_HHMMSS.
*/
string timestamp()
{
    time_t now = time(0); // Get the time.
    tm* ltm = localtime(&now); // Set a pointer to the LOCAL time.

    // Retrieve each member and put them into our string.
    string currenttime = to_string(1900 + ltm->tm_year); 
    currenttime += to_string(1 + ltm->tm_mon);
    currenttime += to_string(ltm->tm_mday);
    currenttime += "_";
    currenttime += to_string(ltm->tm_hour);
    currenttime += to_string(ltm->tm_min);
    currenttime += to_string(ltm->tm_sec);

    // Return the string.
    return currenttime;
}

/* Reads all 3 PCR values and writes them to file. This will need to be updated to also feed data to the UI and be impacted by recipe. 
 */
void readPCR()
{
    // Turn off the cycling LED.
    sens1.LED_off(2);

    // Take our first detector value. Each of these follows the same format, so I'll only comment the first.
    sens1.LED_on(1); // Turn the relevant LED on.
    sens1.setMethod(1); // Set our method to the relevant one.
    sens1.startMethod(); // Start our LED sampling.
    delay(500); // Wait for a sample to be taken--this needs to be OVER 333 ms at least.
    double PCRread = sens1.measure(); //Get the measurement from the qiagen.
    sens1.stopMethod(); // Stop sampling.
    pcr_out << PCRread << ","; // Dump the PCR value to the file and follow it with a comma for csv processing.
    pcrValues.push_back(PCRread); // Push the PCR value onto the result array.
    sens1.LED_off(1); // Turn the LED off.

    sens2.LED_on(1);
    sens2.setMethod(1);
    sens2.startMethod();
    delay(500);
    PCRread = sens2.measure();
    sens2.stopMethod();
    pcr_out << PCRread << ",";
    pcrValues.push_back(PCRread);
    sens2.LED_off(1); 

/*    sens2.LED_on(2);
    sens2.setMethod(3);
    sens2.startMethod();
    delay(500);
    PCRread = sens2.measure();
    sens2.stopMethod();
    pcr_out << PCRread;
    pcrValues.push_back(PCRread);
    sens2.LED_off(2); */

    pcr_out << endl; // Send a new line to the file to get ready for the next cycle.
    sens1.LED_on(2); // Turn the cycling LED back on.
    pcrReady = true; // Say that our PCR readings are ready for output to the gui.
    delay(1000); // Wait for a second to allow the moving average filters to fill back up. 
}