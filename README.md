# fritzbox-callmonitor
by Tobias Link - ranger81@dsuclan.de - www.ranger81.de - see attached LICENSE file

This sketch can be used on a Arduino hooked up to an ENC28J60 ethernet module
or shield. There should be also a LiquidCrystal display (16x2 characters)
attached to the Arduino. It provides a simple call monitoring functionality to
see who is currently calling and who you are going to call. Useful for phones
without own display.

Please check the following website for more information:
https://www.ranger81.de/post-728-FritzBox_CallMonitor_-_Teil_1__Elektronik.htm

**Requirements:**
- Arduino (ATMega w/ 32kByte flash storage)
- ENC28J60 ethernet module or shield
- LC Display HD44780 (16x2 characters)
- AVM FritzBox with enabled CallMonitor support (dial #96*5* to enable)
- UIPEthernet Library (https://github.com/ntruchsess/arduino_uip)

**Last changes:**
- 2014-04-06: Added missed call counter and display of last missed call number on LCD
- 2014-04-05: Added missed call LED feature
- 2014-04-04: Added dim timeout for display backlight and proper time display after DISCONNECT
- 2014-04-03: Added timer for display how long the phone call was
- 2014-04-03: Initial internal release

**Open Topics / ToDo:**
