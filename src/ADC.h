#ifndef ADC_h
#define ADC_h
#include <array>

class adc
{
	private:
		int address;
		std::array<int,16> config;
		int highthresh;
		int lowthresh;
		int pihandle;
		float gainvoltage;
		int maxbitcounts;
		double CalibVoffset=0.0;
		double CalibSlope=1.0;
		void writeconfig();
		void adcsetup();
	public:
		adc(int addr);
		adc(int addr, int ht, int lt);
		adc();  // Default constructor.
		
		void StartConversion();
		void SetMultiplex(int up, int down);
		void SetMultiplex(int up);
		void SetGain(int gain);
		void SetMode(int mode);
		void SetSPS(int sps);
		void SetCompMode(int mode);
		void SetCompPol(int pol);
		void SetLatch(int latch);
		void SetCompQueue(int que);

		int getreading();
		double getvoltage();
		double convertToDegC(double volts);  // Conversion for AD8495 board.
		double convertToEngUnits(double volts);  // Conversion using the calibration in the devices.ini file.
		inline void SetCalibVoffset(float imon_calibVoffset){CalibVoffset = imon_calibVoffset; };
		inline void SetCalibSlope(float imon_calibSlope){CalibSlope = imon_calibSlope; };		

		int gethighthresh();
		int getlowthresh();
		int getconfig();

		float getgainvoltage(){ return gainvoltage; };
		int getmaxbitcounts(){ return maxbitcounts; };

		~adc();
};

#endif
