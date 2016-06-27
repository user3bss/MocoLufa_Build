/*
 * ProjectMain.cpp
 * chip: mega328p
 * Created: 4/21/2016 6:13:50 PM
 *  Author: Brad Steffy
 
 A4 SDA
 A5 SCL
 
 meter for 4 cell lion pack. 16v full charge
 using 100ohm series resistor for uC vin to battery power, this limits heating of uC voltage regulator for >12v
 
 current is measured on ground side of battery.
 
 might add bluetooth support for data upload, but usb serial is option.
 and there's only one serial port, so it might be messy to do anyway.
 
 want to add aH/wH in/out counters
 
 milliohm meter https://www.youtube.com/watch?v=anE0jDeBuxo
 */ 

#include <avr/sleep.h>  // library so it can use various sleep states
#include <EEPROM.h>
#include <Arduino.h>
#include <Wire.h>
#include <U8glib.h>

#define LINE1 10
#define LINE2 23
#define LINE3 36
#define LINE4 49
#define LINE5 62
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);	// I2C / TWI

#define ACS712_Vout_Pin A0
#define VDIV_Vout_Pin A1




/*
adc full scale = 0-5v

60v/120k = 0.0005
5v/10k = .0005
10k/120k = .1

R1: 47k
R2: 15k
R1+R2=62k
(5/R2) * (R1+R2) = full scale input voltage = 20.6v
div = 0.242448
20.6v*30a = 618W max measurement
*/
#define Vdiv 0.1 // = R2/(R1+R2)
//#define max_volts 20.6
//#define max_amps 30
//#define max_watts 618

#define STATUSBAR_WIDTH 126
#define STATUSBAR_HEIGHT 12
void drawSTATUSBAR(uint8_t x, uint8_t y, float percent){
	u8g.drawFrame(x, y, STATUSBAR_WIDTH+x, STATUSBAR_HEIGHT);
	uint8_t fill_width = (STATUSBAR_WIDTH-4) * percent;
	u8g.drawBox(x+2, y+2, fill_width, STATUSBAR_HEIGHT-4);
}

// Sleep Code
void wakeUpNow() {             // here the interrupt is handled after wakeup
	// Wake up actions
	u8g.sleepOff();
	delay(50);
}

// Sleep Setup for IDLE mode
void sleepNow() {                      // here we put the arduino to sleep
	u8g.sleepOn();
	set_sleep_mode(SLEEP_MODE_IDLE);       // sleep mode is set here from the 5 available: SLEEP_MODE_IDLE, SLEEP_MODE_ADC, SLEEP_MODE_PWR_SAVE, SLEEP_MODE_STANDBY,  SLEEP_MODE_PWR_DOWN
	//sleep_enable();                        // enables the sleep bit in the mcucr register so sleep is possible. just a safety pin
	//attachInterrupt(0, wakeUpNow, LOW);  // use interrupt 0 (pin 2) and run function wakeUpNow when pin 2 gets LOW
	sleep_mode();                          // here the device is actually put to sleep!!  THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP
	//sleep_disable();                       // first thing after waking from sleep is disable sleep.
	//detachInterrupt(0);                  // disables interrupt 0 on pin 2 so the wakeUpNow code will not be executed during normal running time.
}

//persistent storage = 1024 bytes
#define battery_ah_addr 0
#define test_float_addr 2
void write_uint16_eeprom(uint16_t v, uint16_t addr){
	uint8_t lowValue = v & 0xFF;
	uint8_t highValue = (v & 0xFF00) >> 8;
	EEPROM.write(addr, lowValue);
	EEPROM.write(addr+1, highValue);
}
uint16_t read_uint16_eeprom(uint16_t addr){
	uint16_t v = EEPROM.read(addr);
	v += EEPROM.read(addr+1) << 8;
	return v;
}
void write_float_eeprom(float v, uint16_t addr){
	eeprom_write_block(&v, &addr, sizeof(v));
}
float read_float_eeprom(uint16_t addr){
	float v;
	eeprom_read_block(&v, &addr, sizeof(v));
	return v;
}

void test_eeprom(){
	Serial.print("battery_ah: "); //longer strings wrap text
	Serial.print(read_uint16_eeprom(battery_ah_addr));
	Serial.print("test_float: "); //longer strings wrap text
	write_float_eeprom(1024.123456, test_float_addr); //displays 1024.12
	Serial.print(read_float_eeprom(test_float_addr));
}




void setup(){
	//pinMode(6, OUTPUT);	  //set output PWM D6 pin high
	//digitalWrite(6, HIGH);
	//delay(5000);
	delay(180);          //delay keeps screen from being random pixels
	Serial.begin(19200); //serial seems to be one greater than specified, use 38400 for link on slave side if 19200 is set here
	delay(20);          //allow serial port to stabilize 
	u8g.begin();
	// assign default color value
	if ( u8g.getMode() == U8G_MODE_R3G3B2 ) {
		u8g.setColorIndex(255);     // white
	}
	else if ( u8g.getMode() == U8G_MODE_GRAY2BIT ) {
		u8g.setColorIndex(3);         // max intensity
	}
	else if ( u8g.getMode() == U8G_MODE_BW ) {
		u8g.setColorIndex(1);         // pixel on
	}
	else if ( u8g.getMode() == U8G_MODE_HICOLOR ) {
		u8g.setHiColorByRGB(255,255,255);
	}

	//test_batt_status();
	//test_text();
	//test_eeprom();
	//delay(1000);
	//sleepNow();
	//Serial.println("setup");
}

float WH = 0;
float AH = 0;
void loop(){
	unsigned long start_time = millis();
	uint16_t VDIV_Vout = analogRead(VDIV_Vout_Pin);
	
    // Convert the raw value being read from analog pin
    uint16_t ACS712_Vout = analogRead(ACS712_Vout_Pin);
/* ACS712ELC-30A Specs
	66mV / A
	5v operation
	40 to 85 degC operation
	35uS power on time
	
	vout = vcc/2 = no current
	vout < vcc/2 = -current
	vout > vcc/2 = +current
	
	(vout - (vcc/2)) / .066 = Imeasure
*/	
    float I = ((ACS712_Vout * (5.0 / 1023)) - 2.5) / .066; //5v vcc 10bit adc
	float V = (VDIV_Vout * (5.0 / 1023)) / Vdiv; //5v vcc 10bit adc	
	float W = V * I;
	WH = (W/3600)+ WH;
	AH = (I/3600)+AH;
	
	u8g.firstPage();
	do {
		u8g.setFont(u8g_font_courB10);
		
		u8g.setPrintPos(0, LINE1);
		u8g.print("V: ");
		u8g.print(V);
		
		u8g.setPrintPos(0, LINE2);
		u8g.print("I: ");
		u8g.print(I);
	
		u8g.setPrintPos(0, LINE3);
		u8g.print("W: ");
		u8g.print(W);
		
		u8g.setPrintPos(0, LINE4);
		u8g.print("AH: ");
		u8g.print(AH);
		
		float perMaxI = I/30;
		if(I<0)
			perMaxI *= -1;
		drawSTATUSBAR(0, 52, perMaxI); //per max amps
	} while ( u8g.nextPage() );
	
	Serial.print("V");	Serial.print(V);
	Serial.print("I");	Serial.print(I);
	//Serial.print("W");	Serial.print(W);
	unsigned long end_time = millis();	
	delay(1000 - (end_time - start_time));
}
