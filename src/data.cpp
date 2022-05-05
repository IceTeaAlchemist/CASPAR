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

using namespace std;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

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
    // Be sure to lock the thread here. This prevents the thread from interfering with global variables while they're being used by other portions of the program.
    piLock(0);
    auto millisecs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    reading now;
    now.timestamp = millisecs;
    now.voltage = D2.getreading();
    data.push_back(now);
    if(x.size() == 0)
    {
        run_start = now.timestamp;
    }
    x.push_back((now.timestamp - run_start)/1000.0);
    double ycurrent =(now.voltage/32768.0)*4096.0;
    if(yaverage.size() >= SMOOTHING)
    { 
        yaverage.pop_front();
    }
    yaverage.push_back(ycurrent);
    double average = mean(yaverage);
    y.push_back(average);
    if(y.size() > SMOOTHING)
    {
        xderivs.push_back((now.timestamp - run_start)/1000.0);
        double deriv = (y[y.size() - 1] - y[y.size() - 2])*25;
        double dderiv = (y[y.size() - 1] - y[y.size() - 2])*25;
        derivs.push_back(deriv);
    }
    reading *ptr = &now;
    fwrite(ptr, sizeof(reading), 1, output);
    piUnlock(0);
}


/* Reads all 3 PCR values and writes them to file. This will need to be updated to also feed data to the UI and be impacted by recipe. 
 */
void readPCR()
{
    sens1.LED_off(2);
    sens1.LED_on(1);
    sens1.setMethod(1);
    sens1.startMethod();
    delay(500);
    double PCRread = sens1.measure();
    sens1.stopMethod();
    pcr_out << PCRread << ",";
    sens1.LED_off(1);

    sens2.LED_on(1);
    sens2.setMethod(1);
    sens2.startMethod();
    delay(500);
    PCRread = sens2.measure();
    sens2.stopMethod();
    pcr_out << PCRread << ",";
    sens2.LED_off(1); 

/*    sens2.LED_on(2);
    sens2.setMethod(3);
    sens2.startMethod();
    delay(500);
    PCRread = sens2.measure();
    sens2.stopMethod();
    pcr_out << PCRread;
    sens2.LED_off(2); */

    pcr_out << endl;
    sens1.LED_on(2);
    delay(1000);
}