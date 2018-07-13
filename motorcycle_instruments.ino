//============================================================================
// Name        : motorcycle_instruments.ino
// Author      : Greg Green <gpgreen@gmail.com>
// Version     : 1.0
// Copyright   : GPL v3
// Description : arduino sketch for motorcycle gauges
//============================================================================

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <EEPROMStore.h>
#include <MotoPanel.h>
#include <Bounce2.h>
#include <FreqMeasure.h>
#include <MsTimer2.h>

EEPROMStore store = EEPROMStore();

// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Teensy
//   for this application, we switch it to pin 14 the alternate (see setup)
// MOSI is LCD DIN - this is pin 11 on Teensy
// MISO is not used - this is pin 12 on Teensy
// CS is not used - this is pin 10 on Teensy
// Select the following pins for the rest of the functions
// pin A1 - Data/Command select (D/C)
// pin 10 - LCD chip select (CS)
// pin 7 - LCD reset (RST)
// Note with hardware SPI MISO and SS pins aren't used but will still be read
// and written to during SPI transfer.  Be careful sharing these pins!
Adafruit_PCD8544 display = Adafruit_PCD8544(A1, 10, 7);

MotoPanel panel = MotoPanel(display);

// the bounce object
Bounce debouncer = Bounce();

// the PWM output pin for LCD backlight
const int backlightPin = 6;

// the button pin
const int buttonPin = 2;

// the hall effect sensor (for FrequencyMeasure)
const int hallEffectPin = 8;

// the external voltage pin
const int extVoltage = A0;

// flag to signal each second
volatile bool g_1_hz = false;

// flag for every millisecond
volatile bool g_1000_hz = false;

// interrupt function of every millisecond
void everyMilliSecond()
{
    static int count = 0;
    g_1000_hz = true;
    if (++count == 1000) {
        g_1_hz = true;
	count = 0;
    }
}

void setup() {
    Serial.begin(115200);

    // initialize the state from EEPROM store
    store.begin();

    // setup the hall effect interrupt
    //pinMode(hallEffectPin, INPUT);
  
    // setup the backlight pwm
    analogWrite(backlightPin, store.backlight());

    // setup the lcd display
    display.begin();
    // you can change the contrast around to adapt the display
    // for the best viewing!
    display.setContrast(store.contrast());
    display.display(); // show splashscreen

    // panel
    panel.begin(store.rpmRange(), store.mileage());
    Serial.print("rpm range:");
    Serial.println(store.rpmRange(), DEC);

    // bounce button
    pinMode(buttonPin, INPUT);
    debouncer.attach(buttonPin);
    debouncer.interval(5);	// interval in ms

    Serial.println("setup finished!");

    // begin timer (every millisecond)
    MsTimer2::set(1, everyMilliSecond);
    MsTimer2::start();
    
    // initalize rpm detector
    FreqMeasure.begin();
}

// the current measured voltage
float g_measured_voltage = 0.0f;

// when the button was pressed
int button_down = -1;

// flag to signal display update
bool updateDisplay = false;

// serial input buffer and current offset into buffer
int bufoffset = 0;
char inputBuffer[128];

// hall effect sensor freq count
double sum = 0;
int count = 0;
//int lastHallEffect = HIGH;
//int mag = 0;

void loop() {
    // check the hall effect sensor
    if (FreqMeasure.available()) {
        // average several readings together
        sum = sum + FreqMeasure.read();
        count = count + 1;
        if (count > 30) {
            float frequency = FreqMeasure.countToFrequency(sum / count);
            Serial.print("freq:");
            Serial.println(frequency);
            sum = 0;
            count = 0;
        }
    }

    // if (digitalRead(hallEffectPin)) {
    //     if (lastHallEffect != HIGH) {
    //         ++mag;
    //         lastHallEffect = HIGH;
    //         Serial.print("mag:");
    //         Serial.println(mag);
    //     }
    // }
    // else {
    //     if (lastHallEffect == HIGH) {
    //         ++mag;
    //         lastHallEffect = LOW;
    //         Serial.print("mag:");
    //         Serial.println(mag);
    //     }
    // }

    // check for serial input
    if (Serial.available() > 0) {
	inputBuffer[bufoffset++] = Serial.read();
	// when we get a newline, try to process command
	if (inputBuffer[bufoffset-1] == '\n')
	    processCommand();
	// no buffer overflows
	if (bufoffset == 1024)
	    bufoffset = 0;
    }
    
    if (g_1_hz) {
	g_1_hz = false;
	// increment mileage for test
	static int count = 0;
	if (count++ == 1) {
	    store.addMileage(1);
	    count = 0;
	}
	
	// mileage
	store.writeMileage();
	panel.setMileage(store.mileage());
	panel.setTrip1Mileage(store.trip1());
	panel.setTrip2Mileage(store.trip2());

	// the voltage
	panel.setVoltage(g_measured_voltage);
    }

    if (g_1000_hz) {
	g_1000_hz = false;
  // increment button count if needed
  if (button_down >= 0)
      ++button_down;
      
	// read the external voltage
	average_voltage(analogRead(extVoltage));

	// change speed
	static int scount = 0;
	static int speed = 0;
	static int up = 1;
	if (scount++ == 200) {
	    speed += up ? 1 : -1;
	    if (speed == 99)
		up = 0;
	    else if (speed == 0 && up == 0)
		up = 1;
	    scount = 0;
	}
	panel.setSpeed(speed);

    	// change rpm
	static int rcount = 0;
	static int rpm = 0;
	static int rup = 1;
	if (rcount++ == 210) {
	    rpm += rup ? 100 : -100;
	    if (rpm == store.rpmRange())
		rup = 0;
	    else if (rpm == 0 && rup == 0)
		rup = 1;
	    rcount = 0;
	}
	panel.setRPM(rpm);
    }

    // check for button state change
    if (debouncer.update()) {
        if (debouncer.fell()) {
            button_down = 0;
        }
        if (debouncer.rose()) {
            if (button_down > 1000) {
                Serial.println("Button long-pressed");
                panel.buttonLongPressed();
            } else {
                Serial.println("Button pressed");
                panel.buttonPressed();
            }
            button_down = -1;
        }
    }

    // check for display update
    if (updateDisplay || panel.loopUpdate()) {
	display.display();
    }
    updateDisplay = false;
}

void average_voltage(int new_reading)
{
    static const int count = 20;
    static int current = 0;
    static int average[count];
    average[current++] = new_reading;
    if (current == count) {
	int sum = 0;
	for (int i = 0; i < count; ++i)
	    sum += average[i];
	g_measured_voltage = ((sum / static_cast<float>(count)) / 65535.0)
	    * store.voltageCorrection() * 3.3;
	current = 0;
    }
}

// serial input commands

void cmd_mileage(int val, float fval)
{
    if (val < 0 || val > 8000000)
	return;
    store.setMileage(val);
    panel.setMileage(store.mileage());
    panel.setTrip1Mileage(store.trip1());
    panel.setTrip2Mileage(store.trip2());
}

void cmd_rpmrange(int val, float fval)
{
    if (val < 0 || val > 30000)
	return;
    store.setRPMRange(val);
    panel.begin(store.rpmRange(), store.mileage());
}

void cmd_contrast(int val, float fval)
{
    if (val < 0 || val > 128)
	return;
    store.setContrast(val);
    display.setContrast(store.contrast());
}

void cmd_backlight(int val, float fval)
{
    if (val < 0 || val > 256)
	return;
    store.setBacklight(val);
    analogWrite(backlightPin, store.backlight());
}

void cmd_voltage(int val, float fval)
{
    if (fval < 0.0 || fval > 10.0)
	return;
    store.setVoltageCorrection(fval);
}

void cmd_metric(int val, float fval)
{
    store.setMetric();
}

void cmd_imperial(int val, float fval)
{
    store.setImperial();
}

void cmd_reseteeprom(int val, float fval)
{
    store.initializeEEPROM();
}

// structure to dispatch commmands
struct CommandDispatch {
    const char* name;
    char valspec;
    void (*func)(int val, float fval);
};

// array of command dispatchers
const struct CommandDispatch commands [] = {
    {"mileage", 'd', cmd_mileage},
    {"rpmrange", 'd', cmd_rpmrange},
    {"contrast", 'd', cmd_contrast},
    {"backlight", 'd', cmd_backlight},
    {"voltage", 'f', cmd_voltage},
    {"metric", 'd', cmd_metric},
    {"imperial", 'd', cmd_imperial},
    {"reseteeprom", 'd', cmd_reseteeprom},
    {0, 0, 0},
};

// process a command received by Serial
// command format:
// command_name '=' command_value '\n'
//
// command_name is first field in command structure
// command_value is interpreted by second field in structure:
//   d is an integer value
//   f is an floating point value
//
// when command is parsed and executed, serial input buffer is
// reset
void processCommand()
{
    Serial.println("process command");
    int i = 0;
    bool processed = false;
    while (1) {
	if (commands[i].name == 0)
	    break;
	const struct CommandDispatch* cmd_struct = &commands[i++];
	const char* cmd = cmd_struct->name;
	int cmdlen = strlen(cmd);
	Serial.println(cmd);
	// buffer must contain command + 2 characters for a match
	if (cmdlen > bufoffset-2)
	    continue;
	// set the trailing null value, replacing the newline
	inputBuffer[bufoffset-1] = 0;
	Serial.println(inputBuffer);
	if (strncmp(cmd, inputBuffer, cmdlen) == 0) {
	    if (inputBuffer[cmdlen] != '=')
		continue;
	    Serial.print("Command received:");
	    Serial.print(cmd);
	    // extract the value from the string between = and end
	    char* valptr = &inputBuffer[cmdlen+1];
	    int val = 0;
	    float fval = 0.0;
	    if (cmd_struct->valspec == 'd') {
		sscanf(valptr, "%d", &val);
		Serial.print(" val:");
		Serial.println(val, DEC);
	    } else {
		sscanf(valptr, "%f", &fval);
		Serial.print(" fval:");
		Serial.println(fval, 4);
	    }
	    // execute the command
	    cmd_struct->func(val, fval);
	    processed = true;
	    break;
	}
    }
    bufoffset = 0;
    if (!processed)
	Serial.println("unrecognized command");
}

