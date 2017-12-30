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

EEPROMStore store = EEPROMStore();

// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Teensy
//   for this application, we switch it to pin 14 the alternate (see setup)
// MOSI is LCD DIN - this is pin 11 on Teensy
// MISO is not used - this is pin 12 on Teensy
// CS is not used - this is pin 10 on Teensy
// Select the following pins for the rest of the functions
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 8 - LCD reset (RST)
// Note with hardware SPI MISO and SS pins aren't used but will still be read
// and written to during SPI transfer.  Be careful sharing these pins!
Adafruit_PCD8544 display = Adafruit_PCD8544(5, 4, 8);

MotoPanel panel = MotoPanel(display);

// IntervalTimers
IntervalTimer secTimer;
IntervalTimer milliTimer;

// the bounce object
Bounce debouncer = Bounce();

// the PWM output pin for LCD backlight
const int backlightPin = 6;

// the button pin
const int buttonPin = 2;

// the hall effect sensor (for FrequencyMeasure)
const int hallEffectPin = 3;

// flag to signal each second
volatile bool g_1_hz = false;

// flag for every millisecond
volatile bool g_1000_hz = false;

// interrupt function for every second
void everySecond()
{
    g_1_hz = true;
}

// interrupt function of every millisecond
void everyMilliSecond()
{
    g_1000_hz = true;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
	; // wait for serial port to connect. Needed for native USB port only
    }

    // initialize the state from EEPROM store
    store.begin();

    // setup the hall effect interrupt
    pinMode(hallEffectPin, INPUT);
  
    // setup the backlight pwm
    analogWrite(backlightPin, store.backlight());

    // switch the SCK pin to alternate
    SPI.setSCK(14);

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

    // begin timers
    secTimer.begin(everySecond, 1000000);
    milliTimer.begin(everyMilliSecond, 1000);

    // initalize rpm detector
    FreqMeasure.begin();
}

// the current measured voltage
float g_measured_voltage = 0.0f;

// the last state of the button
int lastButton = HIGH;

// when the button was pressed
unsigned long button_down_time = 0;

// flag to signal display update
bool updateDisplay = false;

// serial input buffer and current offset into buffer
int bufoffset = 0;
char inputBuffer[1024];

// hall effect sensor freq count
double sum = 0;
int count = 0;
//int lastHallEffect = HIGH;
//int mag = 0;

void loop() {
    // update the bounce instance
    debouncer.update();

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
	// set the analog resolution to max for teensy
	analogReadResolution(16);
	// read the input voltage on pin 15
	average_voltage(analogRead(15));

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

    // button test
    int value = debouncer.read();
    //Serial.print("Value=");
    //Serial.println(value);
    if (lastButton != LOW && value == LOW) {
	lastButton = value;
        button_down_time = millis();
    } else if (lastButton == LOW && value == HIGH) {
	lastButton = value;
        if ((millis() - button_down_time) > 1000) {
            Serial.println("Button long-pressed");
            panel.buttonLongPressed();
        } else {
            Serial.println("Button pressed");
            panel.buttonPressed();
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

