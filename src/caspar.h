#ifndef CASPARMAIN
#define CASPARMAIN

// Code is copyrighted by Nick Spurlock, Bill Gabella, and Rick Haselton, as well as
// Vanderbilt University for 2022-2024.


#include "ADC.h"
#include "qiagen.h"
#include "configINI.h"
#include <deque>
#include <vector>
#include <string>
#include <sys/stat.h> // For the mkdir() function in Linux.
#include <iostream> // For ostringstream for couts.
#include <sstream>  // For stringstream ??
#include <fstream>  // For ifstream below ??
//c++17 #include <filesystem> // For file existence like filesystem::exists("path_to_my_file/my_filename")
// From https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exists-using-standard-c-c11-14-17-c 
inline bool fileExists(const std::string& name) {
    ifstream f(name.c_str(), std::ifstream::in);
    return f.good();
}

// See the extern int's below for these. weg 20221110
// #define DEVICE_ID 0x48
// #define TEMP_ID 0x49
// #define HEATER_PIN 7
// #define FAN_PIN 4
// #define BOX_FAN 5
// #define PWM_PIN 23
// #define SMOOTHING 25
// #define CONVERGENCE_THRESHOLD 1
// #define RT_LENGTH 600
// #define CALIBRATION_MIN 150

const double max_vals[3] = {500, 60, 10};
const double min_vals[3] = {-500, -60, -10};
// const int iter_thresh = 24;
// const int allowed_temp_errors = 3;

//Declare reading struct.
struct reading
{
    long timestamp;
    double voltage;
};

//Declare error class (or struct?).
struct error
{
    long timestamp;
    string progname;
    string message;
};

// Global variable declarations. External variables are set in setup.cpp.
// This should include devices.ini hardware type configurations and the assay recipes.
// Take care not to collide with local variables.
extern adc *ADC0;
extern adc *TEMP;
extern qiagen *sens1;
extern qiagen *sens2;
extern bool gainCalibration;
extern qiagenMap myQiagenMap;  // Mapping of HTP, LTP, etc to Qiagen number and method/channel.
extern vector<reading> data;
extern vector<error> errorArray;  // Above, class with members timestamp, progname, message .
extern vector<double> tempkey;
extern vector<double> fluorkey;
extern vector<double> x;
extern vector<double> y;
extern vector<reading> data0_tempers;
extern vector<reading> data1_tempers;
extern vector<reading> data0_adc0;
extern vector<double> x0_tempers;  // In setup.h and data.cpp.
extern vector<double> y0_tempers;
extern vector<double> x1_tempers;
extern vector<double> y1_tempers;
extern vector<double> x0_adc0;  // For plots that do not exist, yet(?).
extern vector<double> y0_adc0;
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
extern fstream temper_out;
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
extern string temperstorage;
extern string runlogDir;
extern string runlog;
extern int runerror;
extern int temperrors;
extern bool RTflag;
//
extern bool pwm_enable;
extern double pwm_high_ratio;
extern int pwm_high;
extern double pwm_low_ratio;
extern int pwm_low;
extern float pwm_clock;
extern int pwm_range;
extern int pwm_mode;
extern bool temper_enable;
extern int adc1gain;
extern int adc1mode;
extern int adc1sps;
extern int adc1comppol;
extern int adc1compqueue;
extern vector<int> adc1multiplex;
extern float temper_vmax;
extern float temper_pow2effbits;
extern float temper_calibVoffset;
extern float temper_calibSlope;
extern float temper_readeveryms; 
//
extern double phasestart;
extern double phaseend;
extern int cyclecutoff;
extern int cyclesfit;
extern int cyclesaverage;
extern double heattoolongfirst; // secs, 75 typically for first heating cycle.
extern double heattoolong; // secs, 40 typ., second and up heating cycle.
extern double cooltoolong; // secs, 40 typ., cooling cycle check.
extern int allowed_temp_errors; // 3-ish
extern vector<int> LTP;
extern vector<int> HTP;
extern bool single_hump; // If using same HTP == LTP then double hump, single_hump=false.
extern int fittingqiagen;
extern config *devicesIni;  // devices.ini file via config reading, hardware in the box.
extern config *recipeIni;   // xxxx.ini file via config reading, recipes for each assay.
extern string BoxName;
extern string recipeVersion;
extern string recipeDate;
extern string recipeDescShort;
extern string recipeDescLong;
extern int DEVICE_ID;  // Typ 0x48.  Qiagen ADC (ADS115).
extern int TEMP_ID;    // Typ 0x49.  Temperature TC ADC (ADS115).
extern int HEATER_PIN; // These are used a lot of places. Typ 7.
extern int FAN_PIN; // Typ 4
extern int BOX_FAN; // Typ 5
extern int PWM_PIN; // Typ 23
extern int ALERT_PIN;  // Typ 3
extern int SMOOTHING;  // 25
extern int CONVERGENCE_THRESHOLD;  // 1
extern bool RT_Enable;
extern int RT_LENGTH;  // 600
extern int RT_TEMP;
extern int RT_WAITTOTEMP;
extern bool RECON_Enable;
extern int RECON_LENGTH; // 600 
extern int RECON_TEMP;
extern int RECON_WAITTOTEMP;
// extern int CALIBRATION_MIN;  // 150
extern int ITER_MAX; // 24
extern double AMPL_MIN_HTP; // 10
extern double CTR_MIN_HTP; // 2
extern double AMPL_MIN_LTP; // 10
extern double CTR_MIN_LTP; // 2
extern int ITER_MAX_PREMELT;  // For both RECON and RT steps? weg
extern double AMPL_MIN_PREMELT;
extern double CTR_MIN_PREMELT;
// control.cpp L239 .
extern double THRESH; // 0.05  # Looked at LV and recipe for CDZ double hump has 0.05 here, 20221026 weg.
extern double THRESHCOOL; // 0.135
extern double DTHRESHHEAT;  // 0.25 # Other version has 0.25, 20220915 weg.
extern double DTHRESHCOOL; // 0.8  # Other version has 0.8, ditto weg.
// casparapi L65-102.
extern double FluorCalibPremelt; // 150
extern double FluorCalib; // 300
extern double FluorCalibLDNAHTP; // 200
extern double FluorCalibLDNALTP; // 200
//
extern string devicesFile;
extern string devicesDir;
extern string recipeFile;
extern string recipeDir;

// setup.cpp function definitions. These handle the initial set up of each component of the caspar instrument. 
// There is a setup.h now, move these there...if non-extern's.  WEG
void setupPi(void);
void setupQiagen(void);
void openFiles(void);
void setupADC(void);
// Using Qiagen function now.  void calibrategain(void);
void closeFiles();
void doMakeDirs(string longdirname);
// The NAN NODES do not need declaration in header??  void writeComments(string savedComments, string savedStartTime, string savedFinishTime);
void doWriteComments(string savedComments, string savedStartDate, string savedStartTime, string savedFinishTime,
   string savedProjName, string savedOperator, string savedExperimentName);
extern void doHardwareConfig(string filename = "configs/devices.ini");  // Defaults only in header files?? weg
extern void doRecipeConfig(string filename = "configs/recipes/default.ini");
extern void setupPWMLaser();

// heat.cpp function definitions. These handle generation of fluoresence based heat curves and temperature-based control.

double heatquery(double temp);
double fluorquery(double fluor);
void waittotemp(double temp);
void heatgen(const double coeff[4], bool trigger);  // was coeff[3]
void holdtemp(double temp, double time);

// control.cpp function definitions. These handle the control loop for cycling and RT.

void premelt(void);
void runRT(void);
void reconstitute(void);
int cycle(void);
long delaytocycleend(const double coeff[4], double thresh, double ampl_min);  // was coeff[3]
bool modeshift(bool state);
void changeQiagen(vector<int> qiagenproperties);


// data.cpp function definitions. These are for data handling and processing.
double mean(const deque<double> queue);
void clearactivedata(void);
void sampletriggered(void);
void readPCR(void);
string timestamp(void);
void retrieveSample(void);
void retrieveTemperatures(void);

// Needed for the config files with strings like vectors "{0, -1}" .
// Returns the vector of ints {0, -1}, in this example.
vector<int> convertstr2vecint(string);


#endif
