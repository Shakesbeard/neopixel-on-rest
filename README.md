# Neopixel On Rest
This piece of firmware has been designed especially to work with the D.A.L.O.R app for Homey (by Athom).

The D.A.L.O.R app project was created to enable anyone, with a Homey, to easily build any DIY LED device, without the need of any coding skills or embedded controller knowledge. 

Of course it is not limited to that. As the name suggests, it controls LEDs utilizing a simple RESTful state api.

## Current Version
v3.1.3 (Stable)

## Libraries Used
- neopixel library by Adafruit ported for Particle
- webduino library ported for Particle

Do not mind my rusty C++ coding skills. The firmware has been tested extensively and runs stable.
If you want to contribute, feel free to post me suggestions.

## Hardware
The NPOR firmware is designed for the controller family of particle.io only.

### Supported Controllers
- Photon (Verified Working)
- P1
- Electron
- Argon (Verified Working)
- Boron
- Xenon
- RedBear Duo

### Supported LED Types
- TM1803
- SK6812RGBW
- TM1829
- WS2812
- WS2812B
- WS2813
- WS2812B2
- WS2811
- WS2812B_FAST
- WS2812B2_FAST

## Starting Your DIY Project
### What You Need
- A free particle.io account
- One of the supported particle.io controllers
- At least one led or led strip of one of the supported led types
- A signal lifter, a resistor and a capacitor: See the linked hardware guides for details on this

### How To Wire Your Project
Please see the following links for details about what you need to get your hardware set up.

Logic lifter wiring for the neopixel data line: https://learn.adafruit.com/assets/64121

The data line needs to be connected to pin A4 of your particle.io controller.

Adafruit Neopixel Ueberguide for everything you need to know: https://www.digikey.de/en/maker/projects/adafruit-neopixel-berguide/970445a726c1438a9023c1e78c42e0bb

### Flashing The Software
1) Sign up for a free particle.io account if you have not done that, yet.
2) Claim your controller (you can do this with your mobile). Info about how to do this should be included in the package with the controller.

#### Installing from particle.io
3) Go to https://go.particle.io/shared_apps/5fdc7165e6f0b000092dc489 for the latest release of the firmware app.
4) Hit "_Flash_" to install the firmware onto your controller via OTA update. If you have more than one controller, make sure to select the desired target in the "_Devices_" menu first.

#### Installing from GitHub
3) Head over to particle.io and login to the Web IDE.
4) Create a new Particle App.
5) Go to the libraries menu and add the following "_Libraries_" to your app: neopixel, WebServer, MDNS
6) Finally replace your application's code with the npor_firmware.ino code.
7) Hit "_Flash_" to install the firmware onto your controller via OTA update. If you have more than one controller, make sure to select the desired target in the "_Devices_" menu first.

## Pairing With Homey
1) Install the D.A.L.O.R app from Athom's app store.
2) Pair your device with Homey, either as _light_ or as _other_, if you want to use it as a LED dashboard or similiar.

## Final Words
Enjoy your plug'n'play DIY LED toy! :D 