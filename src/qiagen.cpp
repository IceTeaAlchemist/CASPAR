#include <iostream>
#include <string>
#include <cstring>
#include <iomanip>
#include <vector>

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include "qiagen.h"

using namespace std;

/*
 * Default (and for now ONLY) constructor. Takes the filename linux is using to handle the com-port and creates a serial interface. 
 */
qiagen::qiagen(string serial)
{
	serial_port = open(serial.c_str(), O_RDWR);
	if(serial_port < 0)
	{
		cout << "Error " << errno << " from open: " << strerror(errno) << endl;
	}
	else
	{
		cout << "Communicating with " << serial << " on serial port " << serial_port << endl;
		struct termios tty;
		if(tcgetattr(serial_port,&tty) != 0)
		{
			cout << "Error reading attributes: " << strerror(errno) << endl;
		}
		tty.c_cflag &= ~PARENB;
		tty.c_cflag &= ~CSTOPB;
		tty.c_cflag &= ~CSIZE;
		tty.c_cflag |= CS8;
		tty.c_cflag &= ~CRTSCTS;
		tty.c_cflag |= CREAD | CLOCAL;
		tty.c_lflag &= ~ICANON;
		tty.c_lflag &= ~ECHO; // Disable echo
		tty.c_lflag &= ~ECHOE; // Disable erasure
		tty.c_lflag &= ~ECHONL; // Disable new-line echo
		tty.c_lflag &= ~ISIG;
		tty.c_iflag &= ~(IXON | IXOFF | IXANY);
		tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
		tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
		tty.c_oflag &= ~ONLCR;
		tty.c_cc[VTIME] = 0;
		tty.c_cc[VMIN] = 1;
		cfsetispeed(&tty, B57600);
		cfsetospeed(&tty, B57600);
		if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
			cout << "Error setting attributes: " << strerror(errno) << endl;
		}
		address = 0;
		active_method = 1;
	}
}

/*
 * lrc: Calculates the longitudinal redundancy check for the command stored bytewise in arr. Called in both generic read and write.
 */
int qiagen::lrc(vector<unsigned int> arr)
{
	int sum = 0;
	for(int i = 0; i < arr.size(); i++)
	{
		sum += arr[i];
	}
	return 256 - (sum % 256);
}

/*
 * Arguably the most important function short of the constructor. Assembles the command into a string for sending to the serial bus. 
 * Reg is the starting register, rw reflects the read/write status. Command changes based on context, either # of registers or data. 
 */
string qiagen::assemble(unsigned int reg, char rw, vector<unsigned int> command)
{
	string result = ":";
	stringstream ss;
	command.insert(command.begin(),reg % 256);
	command.insert(command.begin(),reg / 256);
	if(rw == 'r')
	{
		command.insert(command.begin(),03);
	}
	else if(rw = 'w')
	{
		command.insert(command.begin(),06);
	}
	else
	{
		cout << "The qiagen only accepts read and write. Please check your usage." << endl;
	}
	command.insert(command.begin(),address);
	int checksum = lrc(command);
	for(int i = 0; i < command.size(); i++)
	{
		ss << setfill('0') << setw(2) << hex << command[i];
	}
	ss << setfill('0') << setw(2) << hex << checksum;
	result.append(ss.str());
	result.append("\r\n");
//	cout << "Command: " << result << endl;
	return result;
}

/*
 * Listens for a response. This reads from the serial port one character at a time until a stop character is received and returns the input.
 */
string qiagen::listen()
{
	string appender;
	int i = 0;
	char in;
	do
	{
		i++;
		read(serial_port, &in, sizeof(in));
		if(in !='\r' && in != '\n' && in != ':')
		{
			appender.append(1,in);
		}
	} while (in != '\n');
//	cout << dec  << i << " bytes heard." << endl;
	return appender;
}

/*
 * Generic read function. Takes the starting register and how many registers are intended to be read, returns the result.
 */
string qiagen::readqiagen(unsigned int reg, unsigned int regs_to_read)
{
	vector<unsigned int> rtr = {regs_to_read/256,regs_to_read%256};
	string command = assemble(reg, 'r', rtr);
	write(serial_port,command.c_str(),command.size());
	string readout = listen();
	return readout;
}

/*
 * Generic write function. Takes the starting register and the data to be read, and returns the confirmation bytes.
 */
string qiagen::writeqiagen(unsigned int reg, vector<unsigned int> data)
{
	string command = assemble(reg,'w',data);
	write(serial_port,command.c_str(),command.size());
	string readout = listen();
	return readout;
}

// Turns the LED on. LED refers to the number of the LED desired.
void qiagen::LED_on(int LED)
{
	writeqiagen(6,{01,00});
	if(LED == 1)
	{
		writeqiagen(514, {00,01});
	}
	if(LED == 2)
	{
		writeqiagen(515, {00,01});
	}
	
}

//Turns the requested LED off.
void qiagen::LED_off(int LED)
{
	writeqiagen(6,{01,00});
	if(LED == 1)
	{
		writeqiagen(514, {00,00});
	}
	if(LED == 2)
	{
		writeqiagen(515, {00,00});
	}
	
}

//Sets the power for the requested LED.
void qiagen::LED_power(int LED, unsigned int power)
{
	if(LED == 1)
	{
		writeqiagen(24, {power,00});
	}
	if(LED == 2)
	{
		writeqiagen(25, {power,00});
	}
	
}

// Sets the minimum power for the requested LED.
void qiagen::LED_min(int LED, unsigned int min)
{
	if(LED == 1)
	{
		writeqiagen(30, {min,00});
	}
	if(LED == 2)
	{
		writeqiagen(31, {min,00});
	}
	
}

// Sets the maximum power for the requested LED.
void qiagen::LED_max(int LED, unsigned int max)
{
	if(LED == 1)
	{
		writeqiagen(28, {max,00});
	}
	if(LED == 2)
	{
		writeqiagen(29, {max,00});
	}
	
}

/*Sets the LED and detector used. Check the manual for a key, but for practical usage:
 * 1 - E1D1
 * 2 - E1D2
 * 3 - E2D2 */
void qiagen::setMethod(unsigned int method)
{
	active_method = method;
	writeqiagen(03, {method,00});
}

void qiagen::setMode(unsigned int mode)
{
	writeqiagen(02, {mode,00});
}

void qiagen::startMethod()
{
	writeqiagen(512, {00, 01});
}

void qiagen::stopMethod()
{
	writeqiagen(513, {00,01});
}

double qiagen::measure()
{
	string val = readqiagen(260+(active_method-1)*2,2);
	int store = stoul(val.substr(6,8),nullptr,16);
	double fluor = store*2500.0/8388607.0;
	cout << "Val: " << fluor << endl;
	return fluor;
} 
	
		

//Closes the serial port. In the future it might be a good idea to have it also return the qiagen to default settings (i.e turning the LEDs off).
qiagen::~qiagen()
{
	LED_off(1);
	LED_off(2);
	close(serial_port);
}
