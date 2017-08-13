#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <EEPROMStore.h>
#include <TimerOne.h>

EEPROMStore store = EEPROMStore();

// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Arduino Uno
// MOSI is LCD DIN - this is pin 11 on an Arduino Uno
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
// Adafruit_PCD8544 display = Adafruit_PCD8544(5, 4, 3);
// Note with hardware SPI MISO and SS pins aren't used but will still be read
// and written to during SPI transfer.  Be careful sharing these pins!
Adafruit_PCD8544 display = Adafruit_PCD8544(5, 4, 3);

void drawSpeed(int spd)
{
  static int last_spd = -1;
  if (spd == last_spd)
    return;

  display.setTextSize(4);
  display.setTextColor(BLACK);
  display.setCursor(24, 10);
  display.print(spd, DEC);
  last_spd = spd;
  display.display();
}

void drawMileage(int mileage)
{
  static int last_mileage = -1;
  int x = mileage;
  if (last_mileage == mileage)
    return;

  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(24, 40);
  int digit = mileage / 1000000;
  x -= digit * 1000000;
  display.print(digit, DEC);
  digit = x / 100000;
  x -= digit * 100000;
  display.print(digit, DEC);
  digit = x / 10000;
  x -= digit * 10000;
  display.print(digit, DEC);
  digit = x / 1000;
  x -= digit * 1000;
  display.print(digit, DEC);
  digit = x / 100;
  x -= digit * 100;
  display.print(digit, DEC);
  digit = x / 10;
  x -= digit * 10;
  display.print(digit, DEC);
  display.print(".");
  display.print(x, DEC);
  last_mileage = mileage;
  display.display();
}

void drawRPM(int rpm)
{
  static int last_rpm = -1;
  if (last_rpm == rpm)
    return;

  long rpm_range = store.rpmRange();
  Serial.print("rpm range:");
  Serial.println(rpm_range, DEC);
  int delta = rpm / (rpm_range/(47 - 12));
  int triy = 47 - delta;
  int trix = rpm / (rpm_range/10);
  Serial.print("rpm tri:");
  Serial.print(trix, DEC);
  Serial.print(",");
  Serial.println(triy, DEC);
  display.fillTriangle(0, 47, 0, triy, trix, triy, BLACK);
  display.drawLine(0, triy, 0, 12, BLACK);
  display.drawLine(0, 12, 10, 12, BLACK);
  display.drawLine(10, 12, trix, triy, BLACK); 
  
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(1, 1);
  display.print(rpm, DEC);
  last_rpm = rpm;
  display.display();
}

void everySecond()
{
  Serial.println("second");
  store.writeMileage();
  drawMileage(store.mileage());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  // initialize the state from EEPROM store
  store.begin();

  display.begin();
  // you can change the contrast around to adapt the display
  // for the best viewing!
  display.setContrast(store.contrast());
  display.display(); // show splashscreen

  // setup timer
  Timer1.initialize(1000000);
  Timer1.attachInterrupt(everySecond);
  display.clearDisplay();
  Serial.println("setup finished!");
  Timer1.start();
}

unsigned long time = 0L;

void loop() {
  drawSpeed(20);
  drawRPM(5000);
}
