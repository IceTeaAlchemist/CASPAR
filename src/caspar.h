#ifndef CASPARMAIN
#define CASPARMAIN
#include "ADC.h"
#include "qiagen.h"
#include <deque>
#include <vector>
#include <string>
#include <sys/stat.h> // For the mkdir() function in Linux.
//#include <iostream> // For ostringstream for couts.

#define DEVICE_ID 0x48
#define TEMP_ID 0x49
#define HEATER_PIN 7
#define FAN_PIN 4
#define BOX_FAN 5
#define PWM_PIN 23
#define SMOOTHING 25
#define CONVERGENCE_THRESHOLD 1
#define RT_LENGTH 600
#define CALIBRATION_MIN 150

const double max_vals[3] = {500, 60, 10};
const double min_vals[3] = {-500, -60, -10};
const int iter_thresh = 24;
const int allowed_temp_errors = 3;

//Declare reading struct.
struct reading
{
    long timestamp;
    double voltage;
};

// Global variable declarations. External variables are set in setup.cpp.
extern adc D2;
extern adc TEMP;
extern qiagen sens1;
extern qiagen sens2;
extern vector<reading> data;
extern vector<double> tempkey;
extern vector<double> fluorkey;
extern vector<double> x;
extern vector<double> y;
extern bool RTdone;
extern bool runflag;
extern vector<double> xderivs;
extern vector<double> derivs;
extern deque<double> yaverage;
extern deque<double> derivaverage;
extern fstream coeff_out;
extern fstream pcr_out;
extern fstream notes_out;
extern fstream runtime_out;
extern int cycles;
extern FILE *output;
extern int datapoints;
extern double run_start;
extern bool recordflag;
extern bool pcrReady;
extern vector<double> pcrValues;
extern string dataDir;
extern string pcrstorage;
extern string notesstorage;
extern string coeffstorage;
extern string rawstorage;
extern string notesstorage;
extern string runlogDir;
extern string runlog;
extern int runerror;
extern int temperrors;
extern bool RTflag;
extern bool pwm_enable;
extern const double pwm_high_ratio;
extern const int pwm_high;
extern const double pwm_low_ratio;
extern const int pwm_low;
extern int cyclecutoff;
extern double heattoolong; // secs, 75 typically, too long? 20221027 weg
extern double cooltoolong; // secs, 75 typically, in control.cpp .

// setup.cpp function definitions. These handle the initial set up of each component of the caspar instrument. 

void setupPi(void);
void setupQiagen(void);
void openFiles(void);
void setupADC(void);
void calibrategain(void);
void closeFiles();
void doMakeDirs(string longdirname);
// In casparapi, void writeComments(string savedComments, string savedStartTime, string savedFinishTime);
void doWriteComments(string savedComments, string savedStartTime, string savedFinishTime,
   string savedProjName, string savedOperator, string savedExperimentName);

// heat.cpp function definitions. These handle generation of fluoresence based heat curves and temperature-based control.

double heatquery(double temp);
double fluorquery(double fluor);
void waittotemp(double temp);
void heatgen(const double coeff[3], bool trigger);
void holdtemp(double temp, double time);

// control.cpp function definitions. These handle the control loop for cycling and RT.

void premelt(void);
void runRT(void);
void reconstitute(void);
int cycle(void);
long delaytocycleend(const double coeff[3], double thresh);
bool modeshift(bool state);


//data.cpp function definitions. These are for data handling and processing.
double mean(const deque<double> queue);
void clearactivedata(void);
void sampletriggered(void);
void readPCR(void);
string timestamp(void);
void retrieveSample(void);




#endif
