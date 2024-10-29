# ADT-Lights
Fish tank light with an LED Strip and additional ambient LEDs controlled by an ESP32 with Annual Daylight Timer (ADT) functionality.

Requires a power supply capable of delivering 4 amps at 5 volts.  Also requires a Qwiic Alphanumeric Display from Sparkfun.

Power Supply:
https://www.digikey.com/en/products/detail/mean-well-usa-inc/LRS-35-5/7705043

Display:
https://www.sparkfun.com/products/16916


Start-up and Configuration:

1.  Power on, wait for clock to display.
2.  Using a BLE Terminal App, find the BLE device "ESP32 LED Bar".
3.  Send your Wifi SSID and password through BLE serial:  To send SSID, send the string ".s[ssid]", where [ssid] is your SSID, for example if my SSID is "FBIVan34" I would send ".sFBIVan34" (without quotes).
4.  To send password, send the string ".p[pass]", where [pass] is your password, for example if my password is "apass" I would send ".papass" (without quotes).  After this command the ESP32 will reboot.
5.  As the ESP32 is booting up after reboot, it will connect to your wifi and display it's IP address.  Reboot to display IP address again if needed by disconnecting and reconnecting power.
6.  In a web browser, type the IP address in the address bar to navigate to the ESP32's webserver and configure the options.
