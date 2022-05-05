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
		int lrc(vector<unsigned int> arr);
		string assemble(unsigned int reg, char rw, vector<unsigned int> command);
	public:
		qiagen(string serial);
		string listen();
		void LED_on(int LED);
		void LED_off(int LED);
		void LED_power(int LED, unsigned int power);
		void LED_max(int LED, unsigned int max);
		void LED_min(int LED, unsigned int min);
		void setMethod(unsigned int method);
		void setMode(unsigned int mode);
		void startMethod();
		void stopMethod();
		double measure();
		string readqiagen(unsigned int reg, unsigned int regs_to_read);
		string writeqiagen(unsigned int reg, vector<unsigned int> data);
		~qiagen();
};

#endif
