#ifndef QIAGEN_h
#define QIAGEN_h
#include <vector>
#include <string>
#include <cstring>

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

using namespace std;

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
	public:
		qiagen(string serial);
		string listen();
		void LED_on(int LED);
		void LED_off(int LED);
		void LED_current(int LED, unsigned int current);
		unsigned int getLED_max(int LED);
		unsigned int getLED_min(int LED);
		void LED_max(int LED, unsigned int max);
		void LED_min(int LED, unsigned int min);
		void setMethod(unsigned int method);
		void setMode(unsigned int mode);
		void startMethod();
		void stopMethod();
		double measure();
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

		~qiagen();
};

#endif
