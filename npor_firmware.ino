/*
    >>> MIT License <<<

Copyright 2020 Thomas Hebendanz

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
IN THE SOFTWARE.
*/

// Logic lifter wiring for the neopixel data line: https://learn.adafruit.com/assets/64121
// Guide: https://www.digikey.de/en/maker/projects/adafruit-neopixel-berguide/970445a726c1438a9023c1e78c42e0bb

#define WEBDUINO_FAIL_MESSAGE ""
#define WEBDUINO_SUPRESS_SERVER_HEADER 1
#include <math.h>
#include <neopixel.h>
#include <WebServer.h>
#include <MDNS.h>

// Identification
const char* Version = "3.1.3";
const int Revision = 1;

// Misc stuff
SYSTEM_MODE(AUTOMATIC);
bool ResetPending = false;
uint8_t SetupOnDefault = 1;
uint8_t Restarted = 1;
struct SettingsObject {
    char cid[10];
    uint16_t pixels;
    uint8_t driver;
    uint8_t colorMode; // 0 = default; 1 = red & green swapped
    uint8_t testAtStartup;
};
SettingsObject mySetup;
unsigned long RestartTimer = 0;

// Neopixel stuff  ---------------------------------------
#define INITIAL_BRIGHTNESS 120 // 0 - 255
Adafruit_NeoPixel* strip = nullptr;

// Neopixel memory stuff ----------------------------------------
struct LedSettings {
    uint8_t red;
    uint8_t gre;
    uint8_t blu;
    uint8_t whi;
};
std::vector<LedSettings> LedMemory;
uint8_t memLastDim = INITIAL_BRIGHTNESS;
LedSettings ArrowLED = LedSettings();

// Neopixel animation data --------------------------------------
struct AniSettings {
    uint8_t redStart;
    uint8_t greStart;
    uint8_t bluStart;
    uint8_t whiStart;
    uint8_t redEnd;
    uint8_t greEnd;
    uint8_t bluEnd;
    uint8_t whiEnd;
    uint8_t aniSpeed;
    uint8_t aniMode;
    int aniProgress;
    uint8_t aniDirection;
};
std::vector<AniSettings> AniMemory;
std::vector<AniSettings> StripAniMemory;
uint8_t StripAniMemory_Speed = 0;
uint8_t StripAniMemory_Mode = 0;
int StripAniMemory_Progress = 0;
uint8_t StripAniMemory_Direction = 0;
Mutex AniLock;

// Neopixel callbacks -------------------------------------------
uint32_t ColorProxy(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    if (mySetup.colorMode == 0) {
        return strip->Color(red, green, blue, white);
    } else if (mySetup.colorMode == 1) {
        return strip->Color(green, red, blue, white);
    } else {
        return strip->Color(0, 0, 0, 0);
    }
}

void setSinglePixel(int ledIndex, uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
{
    AniLock.lock();
    if (ledIndex == -1) {
        killAnyAni();
        for (uint16_t idx = 0; idx < mySetup.pixels; idx++) {
            strip->setPixelColor(idx, ColorProxy(red, green, blue, white));
            LedMemory[idx].red = red;
            LedMemory[idx].gre = green;
            LedMemory[idx].blu = blue;
            LedMemory[idx].whi = white;
            AniMemory[idx].aniMode = 0;
        }
        strip->show();
    } else {
        strip->setPixelColor(ledIndex, ColorProxy(red, green, blue, white));
        strip->show();
        LedMemory[ledIndex].red = red;
        LedMemory[ledIndex].gre = green;
        LedMemory[ledIndex].blu = blue;
        LedMemory[ledIndex].whi = white;
        AniMemory[ledIndex].aniMode = 0; // stop only if we have per pixel animations
    }
    AniLock.unlock();
}

void setBrightness(uint8_t b)
{
    strip->setBrightness(b);
    if (b > 0 && memLastDim == 0) {
        // Restore color states
        for (uint16_t idx = 0; idx < mySetup.pixels; idx++) {
            strip->setPixelColor(idx, ColorProxy(LedMemory[idx].red, LedMemory[idx].gre, LedMemory[idx].blu, LedMemory[idx].whi));
        }
    }
    memLastDim = b;
    strip->show();
}

bool isAnimDataOkay(int ledIdx, int mode, int rs, int gs, int bs, int ws, int re, int ge, int be, int we, int speed) {
    if (ledIdx < -1 || ledIdx >= mySetup.pixels) return false;
    if (rs < 0 || rs > 255) return false;
    if (gs < 0 || gs > 255) return false;
    if (bs < 0 || bs > 255) return false;
    if (ws < 0 || ws > 255) return false;
    if (re < 0 || re > 255) return false;
    if (ge < 0 || ge > 255) return false;
    if (be < 0 || be > 255) return false;
    if (we < 0 || we > 255) return false;
    if (mode < 0 || mode > 2) return false;
    if (speed < 1 || speed > 100) return false;
    return true;
}

void killAnyAni() {
    StripAniMemory_Mode = 1; // 0 = Off
    for (int idx = 0; idx < mySetup.pixels; idx++) {
        AniMemory[idx].aniMode = 0; // 0 = Off
    }
}

void RunTest_Addressing() {
    killAnyAni();
    strip->setBrightness(1);
    for (int i = 0; i < mySetup.pixels; i++)
    {
        strip->setPixelColor(i, ColorProxy(0, 0, 0, 0));
        strip->show();
        delay(4);
        strip->setPixelColor(i, ColorProxy(255, 255, 255, 0));
        strip->show();
        delay(100);
        strip->setPixelColor(i, ColorProxy(0, 0, 0, 0));
        strip->show();
        delay(4);
    }
    strip->setBrightness(memLastDim);
    strip->show();
    delay(500);
}

void RunTest_ColorChannels() {
    killAnyAni();
    strip->setBrightness(1);
    for (int i = 0; i < mySetup.pixels; i++)
    {
        strip->setPixelColor(i, ColorProxy(255, 0, 0, 0));
    }
    strip->show();
    delay(500);
    for (int i = 0; i < mySetup.pixels; i++)
    {
        strip->setPixelColor(i, ColorProxy(0, 255, 0, 0));
    }
    strip->show();
    delay(500);
    for (int i = 0; i < mySetup.pixels; i++)
    {
        strip->setPixelColor(i, ColorProxy(0, 0, 255, 0));
    }
    strip->show();
    delay(500);
    for (int i = 0; i < mySetup.pixels; i++)
    {
        strip->setPixelColor(i, ColorProxy(0, 0, 0, 255));
    }
    strip->show();
    delay(500);
    for (int i = 0; i < mySetup.pixels; i++)
    {
        strip->setPixelColor(i, ColorProxy(0, 0, 0, 0));
    }
    strip->setBrightness(memLastDim);
    strip->show();
    delay(500);
}

void setArrow(uint8_t ledDir, uint8_t mode, uint8_t ledRed, uint8_t ledGre, uint8_t ledBlu, uint8_t ledWhi, uint8_t ledSpeed) {
    AniLock.lock();
    killAnyAni();
    for (int idx = 0; idx < mySetup.pixels; idx++) {
        StripAniMemory[idx].redStart = 0;
        StripAniMemory[idx].greStart = 0;
        StripAniMemory[idx].bluStart = 0;
        StripAniMemory[idx].whiStart = 0;
    }
    ArrowLED.red = ledRed;
    ArrowLED.gre = ledGre;
    ArrowLED.blu = ledBlu;
    ArrowLED.whi = ledWhi;
    StripAniMemory_Speed = ledSpeed;
    if (mode == 0) {
        StripAniMemory_Mode = 2;
    } else {
        StripAniMemory_Mode = 3;
    }
    StripAniMemory_Progress = 0;
    StripAniMemory_Direction = ledDir;
    AniLock.unlock();
}

void setAnimRot(uint8_t direction, uint8_t speed) {
    AniLock.lock();
    killAnyAni();
    if (speed > 0) {
        StripAniMemory_Mode = 1;
        StripAniMemory_Speed = speed;
        StripAniMemory_Progress = 0;
        StripAniMemory_Direction = direction;
    } else {
        StripAniMemory_Mode = 0;
        for (int idx = 0; idx < mySetup.pixels; idx++) {
            strip->setPixelColor(idx, ColorProxy(LedMemory[idx].red, LedMemory[idx].gre, LedMemory[idx].blu, LedMemory[idx].whi));
        }
        strip->show();
    }
    AniLock.unlock();
}

void setAnim(int ledIdx, uint8_t mode, uint8_t rs, uint8_t gs, uint8_t bs, uint8_t ws, uint8_t re, uint8_t ge, uint8_t be, uint8_t we, uint8_t speed) {
    AniLock.lock();
    if (ledIdx == -1) {
        killAnyAni();
        for (int idx = 0; idx < mySetup.pixels; idx++) {
            AniMemory[idx].redStart = rs;
            AniMemory[idx].greStart = gs;
            AniMemory[idx].bluStart = bs;
            AniMemory[idx].whiStart = ws;
            AniMemory[idx].redEnd = re;
            AniMemory[idx].greEnd = ge;
            AniMemory[idx].bluEnd = be;
            AniMemory[idx].whiEnd = we;
            AniMemory[idx].aniSpeed = speed;
            AniMemory[idx].aniMode = mode; // 0 = Off; 1 = Glow; 2 = Blink
            AniMemory[idx].aniProgress = 0;
            AniMemory[idx].aniDirection = 0; // 0 = Forward; 1 = Backward
        }    
    } else {
        AniMemory[ledIdx].redStart = rs;
        AniMemory[ledIdx].greStart = gs;
        AniMemory[ledIdx].bluStart = bs;
        AniMemory[ledIdx].whiStart = ws;
        AniMemory[ledIdx].redEnd = re;
        AniMemory[ledIdx].greEnd = ge;
        AniMemory[ledIdx].bluEnd = be;
        AniMemory[ledIdx].whiEnd = we;
        AniMemory[ledIdx].aniSpeed = speed;
        AniMemory[ledIdx].aniMode = mode; // 0 = Off; 1 = Glow; 2 = Blink
        AniMemory[ledIdx].aniProgress = 0;
        AniMemory[ledIdx].aniDirection = 0; // 0 = Forward; 1 = Backward
    }
    AniLock.unlock();
}

// Data Processing stuff
const unsigned int UPARA_BUFFER_SIZE = 64;

// Zero conf stuff
mdns::MDNS zeroConf;
bool zeroConfSuccess;

// Prep webserver stuff
template<class T>
inline Print &operator <<(Print &obj, T arg)
{ obj.print(arg); return obj; }

// Define webserver host
WebServer webserver("", 80);

void defaultCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
    Serial.print("URL: ");
    Serial.println(url_tail);
    server.httpSuccess();
    server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>Neopixels On Rest v");
    server << Version;
    server << "</h1><h2>Usage</h2>URL SYNTAX SET COLOR: http://###.###.###.###/set/ledIndex/red/green/blue/white to set the pixels. Example: /set/0/255/0/0/0 for bright red.";
    server << "<br />URL SYNTAX SET BRIGHTNESS: http://###.###.###.###/dim/level to set the pixels' brightness. Example: /dim/255 for maximum brightness.";
    server << "<br />URL SYNTAX ANIMATION: http://###.###.###.###/ani/mode/redStart/greenStart/blueStart/whiteStart/redEnd/greenEnd/blueEnd/whiteEnd/speed Example: /ani/0/1/0/0/0/0/0/255/0/0/10";
    server << "<br /> Mode 1 = Glow, Mode 2 = Blink";
    server << "<br /> Speed = 1 - 100";
    server << "</body></html>";
}

void catchCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
    if (strlen(url_tail) > 0) {
        Serial.print("URL: ");
        Serial.println(url_tail);
        Serial.print("TAIL COMPLETE: ");
        Serial.println(tail_complete);
        
        char paramBuf[UPARA_BUFFER_SIZE]; // Pre-allocate buffer for incoming args
        char publishString[40];

        String dataStr = url_tail;
        dataStr.toCharArray(paramBuf, BUFFER_SIZE);
        char *pch = strtok(paramBuf, "/"); // Create string tokenizer.
        if (strcmp ("set", pch) == 0) {
            pch = strtok(NULL, "/"); // skip first element
            int ledIdx = atoi(pch);
            pch = strtok(NULL, "/");
            int ledRed = atoi(pch);
            pch = strtok(NULL, "/");
            int ledGre = atoi(pch);
            pch = strtok(NULL, "/");
            int ledBlu = atoi(pch);
            pch = strtok(NULL, "/");
            int ledWhi = atoi(pch);
            // Para check
            if (ledIdx < -1 || ledIdx >= mySetup.pixels || ledRed < 0 || ledRed > 255 || ledGre < 0 || ledGre > 255 || ledBlu < 0 || ledBlu > 255 || ledWhi < 0 || ledWhi > 255) {
                server.httpFail();
                server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>FAILED</h1>One or more parameters are out of range! Please check your calling URL!");
                server << "</body></html>";
                return;
            }
            // Respond to request..
            server.httpSuccess();
            server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>OK</h1>URL DATA: ");
            server << dataStr;
            server << "<br />IDX: ";
            if (ledIdx == -1) {
                server << "ALL";
            } else {
                sprintf(publishString, "%d", ledIdx);
                server << publishString;
            }
            server << "<br />RED: ";
            sprintf(publishString, "%d", ledRed);
            server << publishString;
            server << "<br />GREEN: ";
            sprintf(publishString, "%d", ledGre);
            server << publishString;
            server << "<br />BLUE: ";
            sprintf(publishString, "%d", ledBlu);
            server << publishString;
            server << "<br />WHITE: ";
            sprintf(publishString, "%d", ledWhi);
            server << publishString;
            server << "</body></html>";
            // Configure desired pixel..
            setSinglePixel(ledIdx, ledRed, ledGre, ledBlu, ledWhi);
        } else if (strcmp ("arr", pch) == 0) {
            pch = strtok(NULL, "/");
            int ledDir = atoi(pch);
            pch = strtok(NULL, "/");
            int ledMode = atoi(pch);
            pch = strtok(NULL, "/");
            int ledRed = atoi(pch);
            pch = strtok(NULL, "/");
            int ledGre = atoi(pch);
            pch = strtok(NULL, "/");
            int ledBlu = atoi(pch);
            pch = strtok(NULL, "/");
            int ledWhi = atoi(pch);
            pch = strtok(NULL, "/");
            int ledSpeed = atoi(pch);
            // Para check
            if (ledMode < 0 || ledMode > 1 || ledDir < 0 || ledDir > 1 || ledRed < 0 || ledRed > 255 || ledGre < 0 || ledGre > 255 || ledBlu < 0 || ledBlu > 255 || ledWhi < 0 || ledWhi > 255 || ledSpeed < 1 || ledSpeed > 100) {
                server.httpFail();
                server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>FAILED</h1>One or more parameters are out of range! Please check your calling URL!");
                server << "</body></html>";
                return;
            }
            // Respond to request..
            server.httpSuccess();
            server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>OK</h1>URL DATA: ");
            server << dataStr;
            server << "<br />RED: ";
            sprintf(publishString, "%d", ledRed);
            server << publishString;
            server << "<br />GREEN: ";
            sprintf(publishString, "%d", ledGre);
            server << publishString;
            server << "<br />BLUE: ";
            sprintf(publishString, "%d", ledBlu);
            server << publishString;
            server << "<br />WHITE: ";
            sprintf(publishString, "%d", ledWhi);
            server << publishString;
            server << "</body></html>";
            // Configure desired pixel..
            setArrow(ledDir, ledMode, ledRed, ledGre, ledBlu, ledWhi, ledSpeed);
        } else if (strcmp ("sta", pch) == 0) {
            server.httpSuccess("application/json");
            server << "{ \"rev\": ";
            sprintf(publishString, "%d", Revision);
            server << publishString;
            server << ", \"fwv\": \"";
            server << Version;
            server << "\", \"dim\": ";
            sprintf(publishString, "%d", memLastDim);
            server << publishString;
            server << ", \"drv\": ";
            sprintf(publishString, "%d", mySetup.driver);
            server << publishString;
            server << ", \"nop\": ";
            sprintf(publishString, "%d", mySetup.pixels);
            server << publishString;
            server << ", \"com\": ";
            sprintf(publishString, "\"%d\"", mySetup.colorMode);
            server << publishString;
            server << ", \"def\": ";
            sprintf(publishString, "%d", SetupOnDefault);
            server << publishString;
            server << ", \"tas\": ";
            sprintf(publishString, "%d", mySetup.testAtStartup);
            server << publishString;
            server << ", \"res\": ";
            sprintf(publishString, "%d", Restarted);
            server << publishString;
            server << " }";
            Restarted = 0;
        } else if (strcmp ("ani", pch) == 0) {
            // /ani/ledIdx/aniMode/rs/gs/bs/ws/re/ge/be/we/speed
            pch = strtok(NULL, "/"); // skip first element
            int ledIdx = atoi(pch);
            pch = strtok(NULL, "/");
            int ledAni = atoi(pch);
            pch = strtok(NULL, "/");
            int ledRed = atoi(pch);
            pch = strtok(NULL, "/");
            int ledGre = atoi(pch);
            pch = strtok(NULL, "/");
            int ledBlu = atoi(pch);
            pch = strtok(NULL, "/");
            int ledWhi = atoi(pch);
            pch = strtok(NULL, "/");
            int ledRedE = atoi(pch);
            pch = strtok(NULL, "/");
            int ledGreE = atoi(pch);
            pch = strtok(NULL, "/");
            int ledBluE = atoi(pch);
            pch = strtok(NULL, "/");
            int ledWhiE = atoi(pch);
            pch = strtok(NULL, "/");
            int aniSpeed = atoi(pch);
            // Validate payload..
            if (isAnimDataOkay(ledIdx, ledAni, ledRed, ledGre, ledBlu, ledWhi, ledRedE, ledGreE, ledBluE, ledWhiE, aniSpeed) == false) {
                server.httpFail();
                server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>FAILED</h1>One or more parameters are out of range! Please check your calling URL!");
                server << "</body></html>";
                return;
            };
            // Respond to request..
            server.httpSuccess();
            server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>OK</h1>URL DATA: ");
            server << dataStr;
            server << "<br />IDX: ";
            if (ledIdx == -1) {
                server << "ALL";
            } else {
                sprintf(publishString, "%d", ledIdx);
                server << publishString;
            }
            if (ledAni == 0) server << "<br />ANIMATION: STOP";
            if (ledAni == 1) server << "<br />ANIMATION: GLOW";
            if (ledAni == 2) server << "<br />ANIMATION: BLINK";
            server << "<br />RED: ";
            sprintf(publishString, "%d - %d", ledRed, ledRedE);
            server << publishString;
            server << "<br />GREEN: ";
            sprintf(publishString, "%d - %d", ledGre, ledGreE);
            server << publishString;
            server << "<br />BLUE: ";
            sprintf(publishString, "%d - %d", ledBlu, ledBluE);
            server << publishString;
            server << "<br />WHITE: ";
            sprintf(publishString, "%d - %d", ledWhi, ledWhiE);
            server << publishString;
            server << "<br />SPEED: ";
            sprintf(publishString, "%d", aniSpeed);
            server << publishString;
            server << "</body></html>";
            setAnim(ledIdx, ledAni, ledRed, ledGre, ledBlu, ledWhi, ledRedE, ledGreE, ledBluE, ledWhiE, aniSpeed);
        } else if (strcmp ("dim", pch) == 0) {
            pch = strtok(NULL, "/"); // skip first element
            int ledDim = atoi(pch);
            // Para check
            if (ledDim < 0 || ledDim > 255) {
                server.httpFail();
                server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>FAILED</h1>One or more parameters are out of range! Please check your calling URL!");
                server << "</body></html>";
                return;
            }
            server.httpSuccess();
            server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>OK</h1>URL DATA: ");
            server << dataStr;
            server << "<br />DIM: ";
            sprintf(publishString, "%d", ledDim);
            server << publishString;
            server << "</body></html>";
            setBrightness(ledDim);
        } else if (strcmp ("rot", pch) == 0) {
            pch = strtok(NULL, "/");
            int direction = atoi(pch);
            pch = strtok(NULL, "/");
            int speed = atoi(pch);
            // Para check
            if (speed < 0 || speed > 100 || direction < 0 || direction > 1) {
                server.httpFail();
                server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>FAILED</h1>One or more parameters are out of range! Please check your calling URL!");
                server << "</body></html>";
                return;
            }
            server.httpSuccess();
            server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>OK</h1>URL DATA: ");
            server << dataStr;
            server << "<br />ANIMATION: ROTATE";
            server << "</body></html>";
            setAnimRot(direction, speed);
        } else if (strcmp ("cfg", pch) == 0) {
            pch = strtok(NULL, "/"); // skip first element
            int numOfPix = atoi(pch);
            pch = strtok(NULL, "/");
            int newDriver = atoi(pch);
            pch = strtok(NULL, "/");
            int colorMode = atoi(pch);
            pch = strtok(NULL, "/");
            int startupSelfTest = atoi(pch);
            // Para check
            if (startupSelfTest < 0 || startupSelfTest > 1 || colorMode < 0 || colorMode > 1 || numOfPix < 1 || numOfPix > 512 || newDriver < 0 || newDriver > 8) {
                server.httpFail();
                server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>FAILED</h1>One or more parameters are out of range! Please check your calling URL!");
                server << "</body></html>";
                return;
            }
            server.httpSuccess();
            server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>OK</h1>URL DATA: ");
            server << dataStr;
            server << "<br />NUMBER OF PIXELS: ";
            sprintf(publishString, "%d", numOfPix);
            server << publishString;
            server << "<br />NEW DRIVER ID: ";
            sprintf(publishString, "%d", newDriver);
            server << publishString;
            server << "<br />NEW COLOR MODE: ";
            sprintf(publishString, "%d", colorMode);
            server << publishString;
            server << "<br />SELFTEST ON STARTUP: ";
            if (startupSelfTest == 1) {
                server << "ENABLED";
            } else {
                server << "DISABLED";
            }
            server << "</body></html>";
            // Save config to EEPROM
            mySetup.pixels = numOfPix;
            mySetup.driver = newDriver;
            mySetup.colorMode = colorMode;
            mySetup.testAtStartup = startupSelfTest;
            EEPROM.put(0, mySetup);
            ResetPending = true;
        } else if (strcmp ("restart", pch) == 0 || strcmp ("reset", pch) == 0 || strcmp ("reboot", pch) == 0) {
            server.httpSuccess();
            server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>OK</h1>Restarting!");
            server << "</body></html>";
            ResetPending = true;
        } else {
            server.httpFail();
            server.printP("<!DOCTYPE html><html><head><title>Neopixels On Rest</title></head><body><h1>FAILED</h1>Please check your calling URL!");
            server << "</body></html>";
            return;
        }
    }
}

// Animation callbacks -----------------------------------------------------------
int getPositionValue(int position, int min, int max) {
    if (max < min) {
        int range = min - max;
        float rangePos = (float)range * (float)position / 10000.0;
        float newVal = (float)max + ((float)range - rangePos);
        return (int)newVal;
    } else {
        int range = max - min;
        float rangePos = (float)range * (float)position / 10000.0;
        float newVal = rangePos + (float)min;
        return (int)newVal;
    }
}

void animateRotation () {
    if (StripAniMemory_Direction == 0) {
        StripAniMemory_Progress += 1;
        if (StripAniMemory_Progress >= mySetup.pixels) {
            StripAniMemory_Progress = 0;
        }
    } else if (StripAniMemory_Direction == 1) {
        StripAniMemory_Progress -= 1;
        if (StripAniMemory_Progress < 0) {
            StripAniMemory_Progress = mySetup.pixels - 1;
        }
    }
    for (int ledIdx = 0; ledIdx < mySetup.pixels; ledIdx++) {
        int newPos = ledIdx + StripAniMemory_Progress;
        if (newPos > mySetup.pixels - 1) newPos -= mySetup.pixels;
        strip->setPixelColor(newPos, ColorProxy(LedMemory[ledIdx].red, LedMemory[ledIdx].gre, LedMemory[ledIdx].blu, LedMemory[ledIdx].whi));
    }
}


int ScaleValue(int level, int value) {
    float newVal = (float)value / 100.0 * (float)level;
    return (int) floor(newVal);
}

void animateArrow_SetPixel (int ledIdx, uint8_t red, uint8_t gre, uint8_t blu, uint8_t whi) {
    if (ledIdx >= 0 && ledIdx < mySetup.pixels) strip->setPixelColor(ledIdx, ColorProxy(red, gre, blu, whi));
}

void animateArrow () {
    // Clear all first
    for (int ledIdx = 0; ledIdx < mySetup.pixels; ledIdx++) {
        strip->setPixelColor(ledIdx, 0, 0, 0, 0);
    }
    int delayBuffer = 2;
    if (StripAniMemory_Mode == 3) delayBuffer = 0;
    // Update arrow
    if (StripAniMemory_Direction == 0) {
        StripAniMemory_Progress += 1;
        if (StripAniMemory_Progress >= mySetup.pixels + delayBuffer) {
            if (StripAniMemory_Mode == 2) {
                StripAniMemory_Progress = 0;
            } else if (StripAniMemory_Mode == 3) {
                StripAniMemory_Direction = 1;
            }
        }
    } else if (StripAniMemory_Direction == 1) {
        StripAniMemory_Progress -= 1;
        if (StripAniMemory_Progress < (delayBuffer * -1)) {
            if (StripAniMemory_Mode == 2) {
                StripAniMemory_Progress = mySetup.pixels - 1;
            } else if (StripAniMemory_Mode == 3) {
                StripAniMemory_Direction = 0;
            }
        }
    }
    animateArrow_SetPixel(StripAniMemory_Progress, ArrowLED.red, ArrowLED.gre, ArrowLED.blu, ArrowLED.whi);
    for (int ledIdx = 0; ledIdx < mySetup.pixels; ledIdx++) {
        if (ledIdx == StripAniMemory_Progress) {
            StripAniMemory[ledIdx].redStart = ArrowLED.red;
            StripAniMemory[ledIdx].greStart = ArrowLED.gre;
            StripAniMemory[ledIdx].bluStart = ArrowLED.blu;
            StripAniMemory[ledIdx].whiStart = ArrowLED.whi;
        } else {
            StripAniMemory[ledIdx].redStart = ScaleValue(50, StripAniMemory[ledIdx].redStart);
            StripAniMemory[ledIdx].greStart = ScaleValue(50, StripAniMemory[ledIdx].greStart);
            StripAniMemory[ledIdx].bluStart = ScaleValue(50, StripAniMemory[ledIdx].bluStart);
            StripAniMemory[ledIdx].whiStart = ScaleValue(50, StripAniMemory[ledIdx].whiStart);
        }
        strip->setPixelColor(ledIdx, StripAniMemory[ledIdx].redStart, StripAniMemory[ledIdx].greStart, StripAniMemory[ledIdx].bluStart, StripAniMemory[ledIdx].whiStart);
    }
}

void animateGlow(int ledIdx) {
    // Calculate current animation position..
    if (AniMemory[ledIdx].aniDirection == 0) {
        AniMemory[ledIdx].aniProgress += AniMemory[ledIdx].aniSpeed;
        if (AniMemory[ledIdx].aniProgress > 10000) {
            AniMemory[ledIdx].aniProgress = 10000;
            AniMemory[ledIdx].aniDirection = 1;
        }
    } else if (AniMemory[ledIdx].aniDirection == 1) {
        AniMemory[ledIdx].aniProgress -= AniMemory[ledIdx].aniSpeed;
        if (AniMemory[ledIdx].aniProgress < 1) {
            AniMemory[ledIdx].aniProgress = 1;
            AniMemory[ledIdx].aniDirection = 0;
        }
    }
    // Calculate current positions rgbw values..
    int newRed = getPositionValue(AniMemory[ledIdx].aniProgress, AniMemory[ledIdx].redStart, AniMemory[ledIdx].redEnd);
    int newGre = getPositionValue(AniMemory[ledIdx].aniProgress, AniMemory[ledIdx].greStart, AniMemory[ledIdx].greEnd);
    int newBlu = getPositionValue(AniMemory[ledIdx].aniProgress, AniMemory[ledIdx].bluStart, AniMemory[ledIdx].bluEnd);
    int newWhi = getPositionValue(AniMemory[ledIdx].aniProgress, AniMemory[ledIdx].whiStart, AniMemory[ledIdx].whiEnd);
    strip->setPixelColor(ledIdx, ColorProxy(newRed, newGre, newBlu, newWhi));
}

void animateBlink(int ledIdx) {
    if (AniMemory[ledIdx].aniDirection == 0) {
        AniMemory[ledIdx].aniProgress += AniMemory[ledIdx].aniSpeed;
        if (AniMemory[ledIdx].aniProgress > 10000) {
            AniMemory[ledIdx].aniProgress = 10000;
            AniMemory[ledIdx].aniDirection = 1;
        }
        strip->setPixelColor(ledIdx, ColorProxy(AniMemory[ledIdx].redStart, AniMemory[ledIdx].greStart, AniMemory[ledIdx].bluStart, AniMemory[ledIdx].whiStart));
    } else if (AniMemory[ledIdx].aniDirection == 1) {
        AniMemory[ledIdx].aniProgress -= AniMemory[ledIdx].aniSpeed;
        if (AniMemory[ledIdx].aniProgress < 1) {
            AniMemory[ledIdx].aniProgress = 1;
            AniMemory[ledIdx].aniDirection = 0;
        }
        strip->setPixelColor(ledIdx, ColorProxy(AniMemory[ledIdx].redEnd, AniMemory[ledIdx].greEnd, AniMemory[ledIdx].bluEnd, AniMemory[ledIdx].whiEnd));
    }
}

// Handle for the animation thread
Thread* AniThread;
os_thread_return_t Thread_Animate() {
    while (true) {
        AniLock.lock();
        bool aniOn = false;
        for (int aniIdx = 0; aniIdx < mySetup.pixels; aniIdx++) {
            // LED wise effect
            if (AniMemory[aniIdx].aniMode == 1) { 
                animateGlow(aniIdx);
                aniOn = true;
            } else if (AniMemory[aniIdx].aniMode == 2) {
                animateBlink(aniIdx);
                aniOn = true;
            }
        }
        if (aniOn == true) strip->show();
        AniLock.unlock();
        delay(1);
    }
}

Thread* AniThread2;
os_thread_return_t Thread_Animate2() {
    while (true) {
        AniLock.lock();
        bool aniOn = false;
        if (StripAniMemory_Mode == 1) {
            animateRotation();
            aniOn = true;
        }
        if (StripAniMemory_Mode == 2 || StripAniMemory_Mode == 3) {
            animateArrow();
            aniOn = true;
        }
        if (aniOn == true) strip->show();
        AniLock.unlock();
        int tDelay = 1020 - (StripAniMemory_Speed * 10);
        delay(tDelay);
    }
}

void setup() {
    Serial.println("Starting..");
    Restarted = 1;
    EEPROM.get(0, mySetup);
    if (strcmp(mySetup.cid, "NPOR3_CV3") != 0) {
        Serial.println("Configuration not found, defaulting..");
        strncpy(mySetup.cid, "NPOR3_CV3\0", 10);
        mySetup.pixels = 1;
        mySetup.driver = 0;
        mySetup.colorMode = 0;
        mySetup.testAtStartup = 1;
    } else {
        Serial.println("Configuration loaded.");
        SetupOnDefault = 0;
    }
    // Setup pixels..
    Serial.println("Initializing Neopixels..");
    for (int i = 0; i < mySetup.pixels; i++) {
        LedMemory.push_back(LedSettings());
        AniMemory.push_back(AniSettings());
        StripAniMemory.push_back(AniSettings());
    }
    StripAniMemory_Mode = 0;
    // Set driver..
    uint8_t actualDriver = WS2812;
    if (mySetup.driver == 1) {
        actualDriver = TM1803;
    } else if (mySetup.driver == 2) {
        actualDriver = SK6812RGBW;
    } else if (mySetup.driver == 3) {
        actualDriver = TM1829;
    } else if (mySetup.driver == 4) {
        actualDriver = WS2812B;
    } else if (mySetup.driver == 5) {
        actualDriver = WS2812B2;
    } else if (mySetup.driver == 6) {
        actualDriver = WS2811;
    } else if (mySetup.driver == 7) {
        actualDriver = WS2812B_FAST;
    } else if (mySetup.driver == 8) {
        actualDriver = WS2812B2_FAST;
    }
    strip = new Adafruit_NeoPixel(mySetup.pixels, A4, actualDriver);
    strip->begin();
    strip->setBrightness(1);
    strip->show(); // Initialize all pixels to 'off'
    delay(200);
    for (int i = 0; i < mySetup.pixels; i++)
    {
        AniMemory[i].aniMode = 0; // init all animation holders to off
        StripAniMemory[i].aniMode = 0; // init all animation holders to off
    }
    setSinglePixel(-1, 0, 0, 0 ,0);
    if (mySetup.testAtStartup == 1) {
        RunTest_Addressing();
        RunTest_ColorChannels();
    }
    strip->setBrightness(INITIAL_BRIGHTNESS);
    strip->show();
    // Set up animator thread..
    AniThread = new Thread("ANI", Thread_Animate);
    AniThread2 = new Thread("ANI2", Thread_Animate2);
    // Setup webserver..
    Serial.println("Initializing the Webserver..");
    webserver.begin();
    webserver.setDefaultCommand(&defaultCmd);
    webserver.setFailureCommand(&catchCmd);
    // Set up zero configuration..
    Serial.println("Initializing the Zeroconf..");
    byte mac[6];
    WiFi.macAddress(mac);
    char macString[13];
    sprintf(macString, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.print("MAC: ");
    Serial.println(macString);
    zeroConfSuccess = zeroConf.setHostname("com-sdn-npor");
    if (zeroConfSuccess) {
        Serial.println("Zeroconf master success.");
        zeroConfSuccess = zeroConf.addService("tcp", "npor3api", 80, "Neopixel On Rest v3");
        zeroConf.addTXTEntry("id", macString);
    }
    if (zeroConfSuccess) {
        Serial.println("Zeroconf service success.");
        zeroConfSuccess = zeroConf.begin(true);
    }
    Serial.println("Ready.");
}

void loop() {
    int UrlBufferLen = 200;
    char UrlBuffer[200];
    webserver.processConnection(UrlBuffer, &UrlBufferLen);
    if (zeroConfSuccess) zeroConf.processQueries();
    if (ResetPending) {
        if (RestartTimer == 0) {
            RestartTimer = millis();
        } else {
            if (millis() - RestartTimer > 5000) System.reset();
        }
    }
}