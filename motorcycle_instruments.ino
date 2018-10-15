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
#include <AnalogDiff.h>

EEPROMStore store = EEPROMStore();

// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Teensy
//   for this application, we switch it to pin 14 the alternate (see setup)
// MOSI is LCD DIN - this is pin 11 on Teensy
// MISO is not used - this is pin 12 on Teensy
// CS is not used - this is pin 10 on Teensy
// Select the following pins for the rest of the functions
// pin 12 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 11 - LCD reset (RST)
// Note with hardware SPI MISO and SS pins aren't used but will still be read
// and written to during SPI transfer.  Be careful sharing these pins!
Adafruit_PCD8544 display = Adafruit_PCD8544(12, 4, 11);

MotoPanel panel = MotoPanel(display);

// the bounce object
Bounce debouncer1 = Bounce();
Bounce debouncer2 = Bounce();

// the PWM output pin for LCD backlight
const int backlightPin = 13;

// the button pins
const int buttonPin1 = 0;
const int buttonPin2 = 2;

// FreqMeasure is used for RPM
// it uses pin 8 on Arduino Uno, and pins 9,10 can't be used for analogWrite
// it uses pin 14 on sanguino

// the hall effect sensor
// this is using interrupt zero, so pin 2 on uno, pin 10 on sanguino
const int hallEffectPin = 10;

// the external voltage pin
const int extVoltage = A0;

// the temperature sensor pin
const int tempSensor = A1;

// flag to signal each second
volatile bool g_1_hz = false;

// flag for every 100 milliseconds
volatile bool g_100_hz = false;

// flag for every millisecond
volatile bool g_1000_hz = false;

// interrupt function called every millisecond
void everyMilliSecond()
{
    static int count1000 = 0;
    static int count100 = 0;
    g_1000_hz = true;
    if (++count100 == 100)
    {
        g_100_hz = true;
        count100 = 0;
    }
    if (++count1000 == 1000)
    {
        g_1_hz = true;
        count1000 = 0;
    }
}

volatile long hallEffectCount = 0L;
static unsigned long speed_t = 0L;

// interrupt function for hall effect sensor when it
// goes high to low
void hallEffectFalling()
{
    ++hallEffectCount;
}

float g_distance_from_on = 0.0f;
float distance_factor = 1.0f;
float temperature_factor = 1.0f;
float temperature_offset = 0.0f;
const float tire_dia = 2.0f;

void adjustForUnits()
{
    if (store.isMetric())
    {
        Serial.println("Speedometer in metric units");
        distance_factor = 3.14159 * tire_dia / 3.2808 / 1000.0;
    }
    else
    {
        Serial.println("Speedometer in imperial units");
        temperature_factor = 9.0 / 5.0;
        temperature_offset = 32.0;
        distance_factor = 3.14159 * tire_dia / 5280.0;
    }
}

void setup()
{
    Serial.begin(115200);

    // initialize the state from EEPROM store
    store.begin();

    // setup the backlight pwm
    analogWrite(backlightPin, store.backlight());

    // setup the lcd display
    display.begin();
    // you can change the contrast around to adapt the display
    // for the best viewing!
    display.setContrast(store.contrast());

    // show splashscreen
    display.display(); 

    // setup the corrections to sensors based on units
    adjustForUnits();
    
    // panel
    panel.begin(store.rpmRange(), store.mileage());
    Serial.print("rpm range:");
    Serial.println(store.rpmRange(), DEC);

    // bounce button 1
    pinMode(buttonPin1, INPUT);
    debouncer1.attach(buttonPin1);
    debouncer1.interval(5);	// interval in ms

    // bounce button 2
    pinMode(buttonPin2, INPUT);
    debouncer2.attach(buttonPin2);
    debouncer2.interval(5);	// interval in ms

    Serial.println("setup finished!");

    // attach the interrupt for hall effect
    pinMode(hallEffectPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(hallEffectPin), hallEffectFalling, FALLING);

    // initalize rpm detector
    FreqMeasure.begin();

    // begin timer (every millisecond)
    MsTimer2::set(1, everyMilliSecond);
    MsTimer2::start();

    speed_t = millis();

    // show the display
    panel.loopUpdate(true);
    display.display();
}

// the current measured voltage
float g_measured_voltage = 0.0f;

// the current measured temperature
float g_measured_temp = 0.0f;

// when the button was pressed
int button1_down = -1;
int button2_down = -1;

// serial input buffer and current offset into buffer
const int bufsize = 256;
int bufoffset = 0;
char inputBuffer[bufsize];

// tach sensor freq count
double sum = 0;
int count = 0;

void loop()
{
    // check the rpm frequency
    if (FreqMeasure.available())
    {
        // average several readings together
        sum = sum + FreqMeasure.read();
        count = count + 1;
        if (count > 30)
        {
            float rpm = FreqMeasure.countToFrequency(sum / count) / 60;
            Serial.print("rpm:");
            Serial.println(rpm);
            panel.setRPM((int)rpm);
            sum = 0;
            count = 0;
        }
    }

    // check for serial input
    if (Serial.available() > 0)
    {
        inputBuffer[bufoffset++] = Serial.read();
        // when we get a newline, try to process command
        if (inputBuffer[bufoffset-1] == '\n')
            processCommand();
        // no buffer overflows
        if (bufoffset == bufsize)
            bufoffset = 0;
    }
    
    if (g_1_hz)
    {
        g_1_hz = false;
        
        static int scount = 0;
        static unsigned long s_last_stored = 0L;
        
        if (++scount == 1)
        {
            // current time
            unsigned long t1 = millis();
            // how many revs since last check
            long count = hallEffectCount;
            // clear the count
            noInterrupts();
            hallEffectCount = 0;
            interrupts();

            // calculate the distance traveled
            float distance = count * store.speedoCorrection() * distance_factor;
            g_distance_from_on += distance;
            unsigned long new_mileage = static_cast<unsigned long>(g_distance_from_on * 10);
            if (new_mileage != s_last_stored)
            {
                store.addMileage(new_mileage - s_last_stored);
                s_last_stored = new_mileage;
            }
            Serial.print("distance:");
            Serial.println(g_distance_from_on, 6);

            // calculate the speed
            float speed = distance / (t1 - speed_t) * 1000 * 3600;
            speed_t = t1;
            panel.setSpeed(speed);

            //Serial.print("speed count:");
            //Serial.println(count);

            scount = 0;
        }

        // update display
        
        // mileage
        store.writeMileage();
        panel.setMileage(store.mileage());
        panel.setTrip1Mileage(store.trip1());
        panel.setTrip2Mileage(store.trip2());
        // the voltage
        panel.setVoltage(g_measured_voltage);
        // the temp
        panel.setTemp(g_measured_temp);
    }

    if (g_100_hz) {
        g_100_hz = false;

        // read the external voltage
        average_voltage(analogRead(extVoltage));

        // read the temperature
        average_temp(AnalogDiff::read());
    }

    if (g_1000_hz)
    {
        g_1000_hz = false;

        // increment button count if needed
        if (button1_down >= 0)
            ++button1_down;
    }

    // check for button state change
    if (debouncer1.update())
    {
        if (debouncer1.fell())
        {
            button1_down = 0;
        }
        if (debouncer1.rose())
        {
            if (button1_down > 1000) {
                Serial.println("Button1 long-pressed");
                panel.buttonLongPressed();
            } else {
                Serial.println("Button1 pressed");
                panel.buttonPressed();
            }
            button1_down = -1;
        }
    }

    // check for display update
    if (panel.loopUpdate())
    {
        display.display();
    }
}

void average_voltage(int new_reading)
{
    static const int count = 20;
    static int current = 0;
    static int average[count];
    //Serial.print("vreading:");
    //Serial.println(new_reading);
    average[current++] = new_reading;
    if (current == count)
    {
        int sum = 0;
        for (int i = 0; i < count; ++i)
            sum += average[i];
        g_measured_voltage = ((sum / static_cast<float>(count)) * 3.30 / 1024)
            * store.voltageCorrection() + store.voltageOffset();
        current = 0;
    }
}

void average_temp(int new_reading)
{
    static const int count = 20;
    static int current = 0;
    static int average[count];
    //Serial.print("treading:");
    //Serial.println(new_reading);
    average[current++] = new_reading;
    if (current == count)
    {
        int sum = 0;
        for (int i = 0; i < count; ++i)
            sum += average[i];
        g_measured_temp = (sum / static_cast<float>(count)) * 330 / 5120;
        g_measured_temp *= temperature_factor;
        g_measured_temp += temperature_offset;
        //Serial.print("temp:");
        //Serial.println(g_measured_temp);
        current = 0;
    }
}

// serial input commands
void cmd_mileage(int val, float fval)
{
    if (val < 0 || val > 9000000)
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
    if (val < 0 || val > 255)
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

void cmd_voltoff(int val, float fval)
{
    if (fval < -10.0 || fval > 10.0)
        return;
    store.setVoltageOffset(fval);
}

void cmd_speed(int val, float fval)
{
    if (fval < -10.0 || fval > 10.0)
        return;
    store.setSpeedoCorrection(fval);
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
struct CommandDispatch
{
    const char* name;
    char valspec;
    void (*func)(int val, float fval);
};

// array of command dispatchers
const struct CommandDispatch commands [] =
{
    {"mileage", 'd', cmd_mileage},
    {"rpmrange", 'd', cmd_rpmrange},
    {"contrast", 'd', cmd_contrast},
    {"backlight", 'd', cmd_backlight},
    {"voltage", 'f', cmd_voltage},
    {"voltoff", 'f', cmd_voltoff},
    {"speed", 'f', cmd_speed},
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
    while (1)
    {
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
        if (strncmp(cmd, inputBuffer, cmdlen) == 0)
        {
            if (inputBuffer[cmdlen] != '=')
                continue;
            Serial.print("Command received:");
            Serial.print(cmd);
            // extract the value from the string between = and end
            char* valptr = &inputBuffer[cmdlen+1];
            int val = 0;
            float fval = 0.0;
            if (cmd_struct->valspec == 'd')
            {
                sscanf(valptr, "%d", &val);
                Serial.print(" val:");
                Serial.println(val, DEC);
            }
            else
            {
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

