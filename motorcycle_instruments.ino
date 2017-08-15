#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <EEPROMStore.h>
#include <TimerOne.h>
#include <MotoPanel.h>

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

MotoPanel panel = MotoPanel(display);

// flag to signal each second
volatile bool g_1_hz = false;

void everySecond()
{
  g_1_hz = true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // initialize the state from EEPROM store
  store.begin();

  // setup the lcd display
  display.begin();
  // you can change the contrast around to adapt the display
  // for the best viewing!
  display.setContrast(store.contrast());
  display.display(); // show splashscreen

  // setup timer
  Timer1.initialize(1000000);
  Timer1.attachInterrupt(everySecond);

  // panel
  panel.begin(store.rpmRange());
  display.clearDisplay();
  Serial.print("rpm range:");
  Serial.println(store.rpmRange(), DEC);
 
  Serial.println("setup finished!");
  Timer1.start();
}

void loop() {
  panel.drawSpeed(20);
  panel.drawRPM(5000);
  if (g_1_hz) {
    g_1_hz = false;
    Serial.println("second");
    store.writeMileage();
    panel.drawMileage(store.mileage());
  }
  if (panel.update())
    display.display();
}
