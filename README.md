# motorcycle_instruments

Digital speedometer for a motorcycle. Runs on custom board emulating an Arduino. The mcu is an avr ATmega644. This project is the Arduino sketch.

The original speedometer on my motorcycle had an analog speed dial, with mechanical odometer and a mechanical trip counter. The digital speedometer has these functions:

- Speed
- Tachometer
- 2 trip meters
- Electrical system voltage measurement
- Outside air temperature
- Display in metric or imperial units

Written by Greg Green
GPL v3.0 see [LICENSE](https://github.com/gpgreen/motorcycle_instruments/blob/master/LICENSE)

## The project uses several external libraries that don't come with Arduino:

- Adafruit-GFX-Library
- Adafruit-PCD8544-Nokia-5110-LCD-library
- [AnalogDiff](https://github.com/gpgreen/AnalogDiff)
- Bounce2
- [EEPROMStore](https://github.com/gpgreen/EEPROMStore)
- elapsedMillis
- FreqMeasure
- [MotoPanel](https://github.com/gpgreen/MotoPanel)
- MsTimer2

## The hardware design for the project resides on github also.

[Speedometer pcb project](https://github.com/gpgreen/speedometer_hardware)

## The project can communicate over the serial port to adjust parameters stored in the EEPROM. The serial port is running at 115200 baud.

Commands. Enter the word, followed by an equals then a number. d is a decimal, f is a floating point. Then type return.

- mileage=d (set the mileage to the number given, the number is the mileage * 10)
- rpmrange=d (set the upper limit of the rpm display between 0 and 30000)
- contrast=d (set the contrast on the Nokia display, between 0 and 128)
- backlight=d (set the backlight intensity on the Nokia display, between 0 and 255)
- voltage=f (set the voltage correction coefficient, between 0 and 10.0)
- voltoff=f (set the voltage correction offset, between -10.0 and 10.0)
- speed=f (set the speed correction coefficient, between -10.0 and 10.0)
- metric=d (set to any number to get metric display)
- imperial=d (set to any number to get imperial display)
- reseteeprom=d (reset the eeprom to default values, erasing all changes, and mileage count)