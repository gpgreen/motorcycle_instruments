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

EEPROMStore store = EEPROMStore();

// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Arduino Uno
//   for this application, we switch it to pin 14 the alternate (see setup)
// MOSI is LCD DIN - this is pin 11 on an Arduino Uno
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
// Adafruit_PCD8544 display = Adafruit_PCD8544(5, 4, 3);
// Note with hardware SPI MISO and SS pins aren't used but will still be read
// and written to during SPI transfer.  Be careful sharing these pins!
Adafruit_PCD8544 display = Adafruit_PCD8544(5, 4, 3);

MotoPanel panel = MotoPanel(display);

// IntervalTimer
IntervalTimer secTimer;
IntervalTimer milliTimer;

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

    // setup the backlight pwm
    analogWrite(6, store.backlight());

    // switch the SCK pin to alternate
    SPI.setSCK(14);

    // setup the lcd display
    display.begin();
    // you can change the contrast around to adapt the display
    // for the best viewing!
    display.setContrast(store.contrast());
    display.display(); // show splashscreen

    // panel
    panel.begin(store.rpmRange());
    display.clearDisplay();
    Serial.print("rpm range:");
    Serial.println(store.rpmRange(), DEC);

    Serial.println("setup finished!");

    // begin timers
    secTimer.begin(everySecond, 1000000);
    milliTimer.begin(everyMilliSecond, 1000);
}

float g_measured_voltage = 0.0f;

void loop() {
    panel.drawSpeed(20);
    panel.drawRPM(5000);
    if (g_1_hz) {
	g_1_hz = false;
	store.writeMileage();
	panel.drawMileage(store.mileage());
	Serial.print("Voltage:");
	Serial.println(g_measured_voltage, 2);
    }
    if (g_1000_hz) {
	g_1000_hz = false;
	// set the analog resolution to max for teensy
	analogReadResolution(16);
	// read the input voltage on pin 15
	average_voltage(analogRead(15));
    }
    if (panel.update())
	display.display();
}

void average_voltage(int new_reading)
{
    static const int count = 20;
    static int current = 0;
    static int average[count];
    average[current++] = new_reading;
    if (current == count) {
	int sum = 0;
	for (int i=0; i<count; ++i)
	    sum += average[i];
	g_measured_voltage = ((sum / static_cast<float>(count)) / 65535.0)
	    * store.voltageCorrection() * 3.3;
	current = 0;
	//Serial.print("Voltage:");
	//Serial.println(g_measured_voltage, 2);
    }
}
