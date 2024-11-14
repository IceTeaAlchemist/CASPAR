#ifndef QIAGEN_h
#define QIAGEN_h
#include <vector>
#include <string>
#include <cstring>

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include "configINI.h" // For the qiagenMap class below, to read recipes directly.
#include "pid.h"  // Also in caspar.h for use elsewhere.
#include <unordered_map> // For the qiagenMap class below.

using namespace std;

// The Qiagen mapping of qiagen number and method to one of
// HTP, LTP, pcrA, pcrB, pcrC, or pcrD.
class qiagenMap
{
	public:  // Start with everything public, once working reconsider this.
		// From the recipe file, must match the [Channels] section of a recipe.
		vector<string> qmPrefix = {"HTP", "LTP", "pcrA", "pcrB", "pcrC", "pcrD", "Recon", "RT"}, 
        	qmSuffix={"", "Qiagen", "Channel", "Name", "Gain"};
		const unordered_map<string, int> qmMethods = { {"E1D1", 1}, {"E1D2", 2}, {"E2D2", 3}, {"E1D1+E1D2", 4}, 
			{"E1D1+E2D2", 5}, {"E1D2+E2D2", 6}, {"E1D1+E1D2+E2D2", 7} };  
			// See ESELog manual page 24, there are more channels, for scope mode.
		inline vector<string> getPrefix(){ return qmPrefix; };
		inline vector<string> getSuffix(){ return qmSuffix; };
		unordered_map<string, int> NameToQiagen, NameToMethod;  // Name is "HTP", "LTP", "pcrA" to "pcrD"
		unordered_map<string, float> NameToGain;
		unordered_map<string, string> qmIniFullMap;
		qiagenMap(config* recipeIni);
		qiagenMap(){};  // Copy constructor??
		void qmPrintAll(); // Print out all the channels.
		vector<int> qmBuildArray(int aa, int bb);  // For building {1,3}, etc we use elsewhere.
		vector<int> qmBuildArray(string Qiagen, string Method); // Inputs as in above strings.
};

// Qiagen hardware class.
class qiagen
{
	private:
		int address;
		struct termios tty;
		int serial_port;
		unsigned int active_method;
		vector<unsigned int> LED_Currents; // {LED1_Current, Curr Def, Curr Max,  Curr Min, LED2_Current...} 
		string BoardName;
		string BoardSerialNumber;
		string HardwareRevision;
		string BoardID;
		string OpticRevision;
		string SoftwareVersion;
		int lrc(vector<unsigned int> arr);
		string assemble(unsigned int reg, char rw, vector<unsigned int> command);
		vector<float> PIDNos;
	public:
		qiagen(string serial);
		qiagen();
		
		string listen();
		void LED_on(int LED);
		void LED_off(int LED);
		void LED_current(int LED, unsigned int current);
		unsigned int getLED_max(int LED);
		unsigned int getLED_min(int LED);
		void LED_max(int LED, unsigned int max);
		void LED_min(int LED, unsigned int min);
		void setMethod(unsigned int method);
		unsigned int getMethod();
		void setMode(unsigned int mode);
		void startMethod();
		void stopMethod();
		double measure();
		vector<double> measureMultiple();
		string readqiagen(unsigned int reg, unsigned int regs_to_read);
		string writeqiagen(unsigned int reg, vector<unsigned int> data);
		void fill_LED_Currents();
		inline vector<unsigned int> get_LED_Currents(){return LED_Currents;}
		enum {LED1act, LED1def, LED1max, LED1min, LED2act, LED2def, LED2max, LED2min };
		void fill_BoardName();
		inline string getBoardName(){return BoardName;}
		void fill_BoardSerialNumber();
		inline string getBoardSerialNumber(){return BoardSerialNumber;}
		void fill_HardwareRevision();
		inline string getHardwareRevision(){return HardwareRevision;}
		void fill_BoardID();
		inline string getBoardID(){return BoardID;}
		void fill_OpticRevision();
		inline string getOpticRevision(){return OpticRevision;}
		void fill_SoftwareVersion();
		inline string getSoftwareVersion(){return SoftwareVersion;}
		int calibrateGain(int minimum_reading, int method);
		void setPIDNos(vector<float> PIDNos);
		inline vector<float> getPIDNos(){return PIDNos;}
	
		int calibrateGainOld(int minimum_reading, int method);
		int calibrateGainOld2(int minimum_reading, int method);

		unsigned int read_average();
		void write_average(unsigned int average);

		~qiagen();
};

#endif
