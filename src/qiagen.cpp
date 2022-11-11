#include <iostream>
//#include <sstream> // ??For ostringstream? But maybe not, maybe from iostream in my test code.
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
 * Default (and for now ONLY) constructor. Takes the filename linux is using to handle the com-port 
*  and creates a serial interface. 
 */
qiagen::qiagen(string serial)
{
	string progName = "qiagen::qiagen";
	//string astring;
	ostringstream bstream;

	serial_port = open(serial.c_str(), O_RDWR);
	if(serial_port < 0)
	{
		bstream << progName << ":**Error: " << errno << " from open: \n\t" << strerror(errno);
		bstream << '\n' << '\tserial is ' << serial << '\n';
		cout << bstream.str();
	}
	else
	{
		cout << progName << ":  Communicating with " << serial << " on serial port "; 
		cout << serial_port << endl;

		struct termios tty;
		if(tcgetattr(serial_port,&tty) != 0)
		{
			cout << progName << ":**Error: reading attributes " << strerror(errno) << endl;
			cout << "\tAttributes tty.c_cflag is " << tty.c_cflag << endl;
		}
		tty.c_cflag &= ~PARENB;  // In system termios-c_cflg.h as DEFINES.
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
		if (tcsetattr(serial_port, TCSANOW, &tty) != 0) 
		{
			cout << progName << ":**Error: setting attributes " << strerror(errno) << endl;
			cout << "\tAttributes tty.c_cflag is " << tty.c_cflag << endl;
		}
		address = 0;
		active_method = 1;

		// Fill some default vectors of information for the Qiagen.
		fill_LED_Currents();  // Fills the vector<unsigned int> LED_currents;
		fill_BoardName();     // Fills the string BoardName; 
		fill_BoardSerialNumber();  // Fills the string BoardSerialNumber;
		fill_BoardID();       // Fills the string BoardID; 
		fill_HardwareRevision();   // Fills the string HardwareRevision;
		fill_OpticRevision(); // Fills the string OpticRevision;
		fill_SoftwareVersion();    // Fills the string OpticRevision;

		cout << progName << ":  BoardID " << getBoardID() << " SerialNumber " << getBoardSerialNumber() << endl;

		if (true)
		{
			cout << "\tLED_Currents, actual, default, max, min" << endl;			
			for (int ii=0; ii < LED_Currents.size()/2; ii += 1)// Could use getLEDCurrents() but seems to work as is.
			{
				cout << "\t" << LED_Currents[ii] << "\t" << LED_Currents[ii+4] << endl;
			}
		}
		if (false) // Change to 1 to execute prints below.
		{
			cout << progName << ":  BoardName is: \n" << BoardName << endl;
			cout << progName << ":  BoardSerialNumber is: " << BoardSerialNumber << endl;
			cout << progName << ":  BoardID is: " << BoardID << endl;
			cout << progName << ":  HardwareRevision is: " << HardwareRevision << endl;
			cout << progName << ":  OpticRevision is: " << OpticRevision << endl;
			cout << progName << ":  SoftwareVersion is: " << SoftwareVersion << endl;
		}

	}
}

/*
 * lrc: Calculates the longitudinal redundancy check for the command stored bytewise in arr. 
 * Called in both generic read and write.
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
 * Arguably the most important function short of the constructor. Assembles the command into a 
 * string for sending to the serial bus. 
 * Reg is the starting register, rw reflects the read/write status. Command changes based on 
 * context, either # of registers or data. 
 */
string qiagen::assemble(unsigned int reg, char rw, vector<unsigned int> command)
{
	string progName = "qiagen::assemble";
	string result = ":";
	stringstream ss;
	command.insert(command.begin(),reg % 256);
	command.insert(command.begin(),reg / 256);
	if(rw == 'r')
	{
		command.insert(command.begin(),03);
	}
	else if(rw == 'w')
	{
		command.insert(command.begin(),06);
	}
	else
	{
		cout << progName << ": **Error: The qiagen only accepts read and write. Please check your usage." << endl;
		// cout << "The qiagen only accepts read and write. Please check your usage." << endl;
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
 * Listens for a response. This reads from the serial port one character at a time until 
 * a stop character is received and returns the input.
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
 * Generic read function. Takes the starting register and how many registers are intended 
 * to be read, returns the result.
 */
string qiagen::readqiagen(unsigned int reg, unsigned int regs_to_read)
{
	vector<unsigned int> rtr = {regs_to_read/256,regs_to_read%256};
	string command = assemble(reg, 'r', rtr);
	write(serial_port,command.c_str(),command.size());
	string readout = listen();  // listen() does NOT read out the : or /r or /n chars!!
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

// Turns the requested LED off.
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

// Sets the current for the requested LED.
// Check against max and min current allowed.
void qiagen::LED_current(int LED, unsigned int current)
{
	int reg;
	unsigned int max, min;
	vector<unsigned int> LEDstuff = get_LED_Currents(); // Reads the data structure.
	if (LED == 1) {
		reg = 24;
		max = LEDstuff[LED1max];
		min = LEDstuff[LED1min];
	}
	else if (LED == 2) {
		reg = 25;
		max = LEDstuff[LED2max];
		min = LEDstuff[LED2min];
	} else {
		cout << "qiagen::LED_current: ***Error: requested incorrect LED " << LED;
		cout <<  " (1 or 2 allowed), doing nothing." << endl;
		return;
	}
	// Check limits on current.
	if ( current <= max && current >= min ){
		writeqiagen(reg, {current, 00});
	} else {
		cout << endl << "qiagen::LED_current: ***Error: requested invalid current " << current;
		cout << " min, max are " << min << ", " << max << " ." << endl; 
		return;

	}
	

	
}
// Get the minimum power for the requested LED.
unsigned int qiagen::getLED_min(int LED)
{
	int reg;
	if (LED == 1) reg = 30;
	else if (LED == 2) reg = 31;
	else {
		cout << "qiagen::getLED_min: ***Error: requested incorrect LED " << LED;
		cout << " (1 or 2 allowed)." << endl;
		return 999;  // Return at very high value so that you cannot pass a minimum check.
	}
	// String return like: :'00'03'02'HH'LL'RC'\r\n'  (my ticks and frame start is :)
	//                            2 bytes, HH high byte, LL low byte (not used), RC is LRC code.
	string val = readqiagen(reg, 1);
	unsigned int store = stoul(val.substr(6,2),nullptr,16);
	return store;	
}

// Get the maximum power for the requested LED.
unsigned int qiagen::getLED_max(int LED)
{
	int reg;
	string astring;
	if (LED == 1) reg = 28;
	else if (LED == 2) reg = 29;
	else {
		cout << "qiagen::getLED_max: ***Error: requested incorrect LED " << LED;
		cout <<  " (1 or 2 allowed)." << endl;
		return -999; // Return a very low value so that you cannot pass a maximum check.
	}
	// String return like: :'00'03'02'HH'LL'RC'\r\n'  (my ticks and frame start is :)
	//                            2 bytes, HH high byte, LL low byte (not used), RC is LRC code.
	string val = readqiagen(reg, 1);
	unsigned int store = stoul(val.substr(6,2),nullptr,16);
	return store;
	
}


// Sets the minimum power for the requested LED.
// Should not be allowed, set a the factory for their reasons 
// (linearity? feedback control?).
void qiagen::LED_min(int LED, unsigned int min)
{
	if(LED == 1)
	{
		//writeqiagen(30, {min,00});
	}
	if(LED == 2)
	{
		//writeqiagen(31, {min,00});
	}
	
}

// Sets the maximum power for the requested LED.
void qiagen::LED_max(int LED, unsigned int max)
{
	if(LED == 1)
	{
		//writeqiagen(28, {max,00});
	}
	if(LED == 2)
	{
		//writeqiagen(29, {max,00});
	}
	
}

/*Sets the LED and detector used. Check the manual for a key, but for practical usage:
 * 1 - E1D1   4 - E1D1+E1D2   7 - E1D1+E1D2+E2D2   10 - S_E2D2 (scope mode)
 * 2 - E1D2   5 - E1D1+E2D2   8 - S_E1D1 (scope mode)
 * 3 - E2D2   6 - E1D2+E2D2   9 - S_E1D2 (scope mode)
 */
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
	int store = stoul(val.substr(6,8),nullptr,16);  //substr, position 6 and 8 chars long
	double fluor = store*2500.0/8388607.0;  // in mV
//	cout << "Val: " << fluor << endl;
	return fluor;
} 

// Loads data into the private vector<unsigned int> LED_Currents, with the
// ordering given below by the order of the registers, 
//  LED1 current, current default, current max, current min,
//	LED2 (ditto).
// The max and minima should not change, the other two could.
void qiagen::fill_LED_Currents(){
	vector<int> regs = {24, 26, 28, 30, 25, 27, 29, 31};
	string val;
	unsigned int store;
	for ( auto& areg: regs) {
		val = readqiagen(areg, 1);
		store = stoul(val.substr(6,2),nullptr,16);
		LED_Currents.push_back(store);
	}
}
// Loads the BoardName from the Qiagen into the data structure to hold it,
// string BoardName;
void qiagen::fill_BoardName(){
	string val = readqiagen(128, 16);
	// cout << "qiagen::fill_BoardName(): XX" << val << "XX";
	int numBytes = stoul( val.substr(4,2), nullptr, 16);
	string ascii = "";
	for ( int ii=0; ii<2*numBytes; ii+=2 ){  // This iterates over characters, takes two hex chars.
		ascii += char( stoul( val.substr(6+ii,2), nullptr, 16) );
	}	
	BoardName = ascii;
}

// Loads the Board Serial Number from the Qiagen into the data structure
// string BoardSerialNumber;
void qiagen::fill_BoardSerialNumber(){
	string val = readqiagen(144, 4);
	//cout << "qiagen::fill_BoardSerialNumber: Info, full readback from Qiagen is \n" << val << endl;
	int numBytes = stoul( val.substr(4,2), nullptr, 16);
	string ascii = "";
	for ( int ii=0; ii<2*numBytes; ii+=2 ){ // This iterates over characters, takes two hex chars.
		ascii += char( stoul( val.substr(6+ii,2), nullptr, 16) );
	}	
	BoardSerialNumber = ascii;

}

// Loads the Hardware Revision Number from the Qiagen into the data structure
void qiagen::fill_HardwareRevision(){
	string val = readqiagen(156, 4);
	//cout << "qiagen::fill_HardwareRevison: Info, full readback from Qiagen is \n" << val << endl;
	int numBytes = stoul( val.substr(4,2), nullptr, 16);
	string ascii = "";
	for ( int ii=0; ii<2*numBytes; ii+=2 ){ // This iterates over characters, takes two hex chars.
		ascii += char( stoul( val.substr(6+ii,2), nullptr, 16) );
	}	
	HardwareRevision = ascii;

}

// Loads the BoardID from the Qiagen into the data structure
void qiagen::fill_BoardID(){
	string val = readqiagen(148, 8);
	//cout << "qiagen::fill_BoardID: Info, full readback from Qiagen is \n" << val << endl;
	int numBytes = stoul( val.substr(4,2), nullptr, 16);
	string ascii = "";
	for ( int ii=0; ii<2*numBytes; ii+=2 ){ // This iterates over characters, takes two hex chars.
		ascii += char( stoul( val.substr(6+ii,2), nullptr, 16) );
	}	
	BoardID = ascii;
}

// Loads the OpticRevision from the Qiagen into the data structure
void qiagen::fill_OpticRevision(){
	string val = readqiagen(160, 4);
	int numBytes = stoul( val.substr(4,2), nullptr, 16);
	string ascii = "";
	for ( int ii=0; ii<2*numBytes; ii+=2 ){ // This iterates over characters, takes two hex chars.
		ascii += char( stoul( val.substr(6+ii,2), nullptr, 16) );
	}	
	OpticRevision = ascii;
}

// Loads the SoftwareVersion from the Qiagen into the data structure
void qiagen::fill_SoftwareVersion(){
	string val = readqiagen(384, 16);
	//cout << "qiagen::fill_SoftwareVersion: Info, full readback from Qiagen is \n" << val << endl;
	int numBytes = stoul( val.substr(4,2), nullptr, 16);
	string ascii = "";
	for ( int ii=0; ii<2*numBytes; ii+=2 ){ // This iterates over characters, takes two hex chars.
		ascii += char( stoul( val.substr(6+ii,2), nullptr, 16) );
	}	
	SoftwareVersion = ascii;
}

int qiagen::calibrateGain(int minimum_reading, int method){
	LED_off(1);
    LED_off(2);
    setMethod(method);
	int LED;
	if(method == 3)
	{
		LED = 2;
	}
	else
	{
		LED = 1;
	}
    writeqiagen(0, {255,255});
    // Start at our minimum gain for this qiagen.
    int mingain = getLED_min(LED);
	int maxgain = getLED_max(LED);
	int basegain = mingain;
    double readinval;
    do
    {
        if(basegain > maxgain) // If our gain outstrips the limits of the qiagen, exit the loop 
        // and log that we're pushing the limits of our optics.
        {
            cout << "qiagen::calibrateGain: Couldn't find a suitable gain. Sample not present or illprepared." << endl;
			cout << "\tGain min, max " << mingain << ", " << maxgain << " and basegain final at " << basegain << " ." << endl;
			LED_off(LED);
            return 1;
        }
        // Log to console what gain is being tested.
        cout << "qiagen::calibrateGain: Testing at gain of: " << basegain << "." << endl;
        // Turn the LED, adjust the current, and turn it back on.
        LED_off(LED);
        LED_current(LED,basegain);
        LED_on(LED);
        startMethod();
        usleep(400000);
        // Measure what gain we receive.
        readinval = measure();
        // Log what reading we receive to console.
        cout << "qiagen::calibrateGain: Reading fluor of: " << readinval << endl;
        // Increase the gain.
        basegain += 10;
        stopMethod();
    } while (readinval < minimum_reading); // Continue while we haven't reached our requested minimum calibration fluoresence.
	cout << "qiagen::calibrateGain: Gain calibrated for LED " << LED << "." << endl; 
	LED_off(LED);
	return 0;
}

//Closes the serial port. In the future it might be a good idea to have it also return the qiagen to default settings (i.e turning the LEDs off).
qiagen::~qiagen()
{
	LED_off(1);
	LED_off(2);
	close(serial_port);
}
