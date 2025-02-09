# ADT-Lights

Fish tank light with an LED Strip and additional ambient LEDs controlled by an ESP32 with Annual Daylight Timer (ADT) functionality.

Requires a power supply capable of continuously delivering 4 amps at 5 volts.  Also requires an LED Strip, UV LEDs, a Qwiic Pro Mini microcontroller, and a Qwiic Alphanumeric Display.

Power Supply:
https://www.digikey.com/en/products/detail/mean-well-usa-inc/LRS-35-5/7705043

LED Strip:
https://www.sparkfun.com/products/14015

UV LEDs:
https://www.sparkfun.com/products/8662

Qwiic Pro Mini:
https://www.sparkfun.com/products/23386

Display:
https://www.sparkfun.com/products/16916

All other components are listed in the BOM.  Hardware size is M3x16mm.


Start-up and Configuration:

1.  Power on, wait for clock to display.
2.  Using a BLE Terminal App on your phone, find the BLE device "ESP32 LED Bar".
3.  Send your Wifi SSID and password through BLE serial:  To send SSID, send the string ".s[ssid]", where [ssid] is your SSID, for example if my SSID is "FBIVan34" I would send ".sFBIVan34" (without quotes).  Your terminal app may give errors, ignore them.
4.  To send password, send the string ".p[pass]", where [pass] is your password, for example if my password is "apass" I would send ".papass" (without quotes).  After this command the ESP32 will reboot.
5.  After reboot, the ESP32 will connect to your wifi and display it's IP address.  Reboot to display the IP address again if needed by disconnecting and reconnecting power.
6.  In a web browser, type the IP address in the address bar to navigate to the ESP32's webserver and configure the options.


  There are two lighting systems - ambient LEDs, and the LED strip.  The ambient LEDs are not addressable, but they are separated into UV and white channels.  The LED strip is addressable and is configured in a U shape with the 2nd half of the strip turned around and arranged next to the first half, so the strip is two LEDs thick.

  Of the 5 PCBs, one is almost fully populated with components except for two of the LED switching channels (BJT+FET) that arent' being used, while the other 4 PCBs are only populated with LEDs and their current limiting resistors.  Wires carry the drive current to the LEDs on each successive board.  The center LEDs have 3 channels, but they are all white, so I connected these together at the connector (J6 or J7), so I only have two channels (this is why "white" shows up in the code indexed as STRIP_R - I'm using the Red channel for all three).  Originally I wanted a bright white LED, so this is what I shopped for, but I noticed in the datasheet that it has 3 LEDs which are all broken out individually just like an RGB, so I made each one separately dimmable.  It's possible to swap the white LED for an RGB LED and use all 4 channels with minimal changes to the code, but for now everything is made to work with the parts listed using only the two channels for ambient lighting (UV and red for white).  So on the bottom side of the PCB, there are 4 clusters of components which are the BJTs and FETs which step up the signal from the uC and drive the LED channels.  Only the bottom two clusters, the ones that connect to pins 25 and 26 on the Qwiic Pro Mini, need to be populated.

  The PCB which is mostly populated also connects to and supplies power to the LED strip.  The last PCB at the opposite end of the bar is where the middle of the LED strip is split and connected in a U-turn to run back down the bar.  So there are a lot of footprints on the PCBs, but a lot of them are left unpopulated on most of the boards and are only used for specific purposes.  The LED strip will go over some of the unpopulated footprints.  It is important to keep the double sided sticky tape on the LED strip as insulation to prevent contact with the unpopulated footprints on the PCB, and also to secure the strip.

  A couple of things to note, the UV LEDs (sparkfun) have a less than ideal viewing angle (45 deg. on datasheet) that is essentially a narrow beam, so they don't fill the space well and give a striped look.  If there is a UV LED in the same package that has a higher viewing angle, that would be an improvement.  Also, a waterproof closure with UV transmissive acryllic is needed.

![alt_text](https://github.com/RichardO82/ADT-Lights/blob/main/Fusion/Tracks.jpg)
![alt_text](https://github.com/RichardO82/ADT-Lights/blob/main/SetupScreen.jpg)
![alt_text](https://github.com/RichardO82/ADT-Lights/blob/main/FadersScreen.jpg)
![alt text](https://github.com/RichardO82/ADT-Lights/blob/main/Fusion/ADT-Lights-SCH.jpg)
![alt_text](https://github.com/RichardO82/ADT-Lights/blob/main/ADT-Lights.jpg)
