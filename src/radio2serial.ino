//radio2serial.ino
//version: 0.55
//github url: http://smarturl.it/radio2serial
//
//Commands are sent with a REST syntax (ex: /radio/text/Hello World)
//Data are sent in JSON (ex: "{"data" : "/radio/text/Hello World"}")
//Protocols managed: RemoteTransmitter (old)/NewRemoteTransmitter (new)/RadioHead (text)

// Library
// RadioHead : http://www.airspayce.com/mikem/arduino/RadioHead/
// 433mhzforarduino : https://bitbucket.org/fuzzillogic/433mhzforarduino/
// FastLed : http://fastled.io/

//PinOut
//ASK 433Mhz Transmitter                : 10
//ASK 433Mhz Receiver (RXB6 recommanded): 2
//Neopixel (WS2812b)                    : 4

//Radio
//Radio Switches
#include <NewRemoteTransmitter.h>
#include <RemoteTransmitter.h>
#include <RemoteReceiver.h>
#include <NewRemoteReceiver.h>
#include <InterruptChain.h>

//RadioHead
#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile

//Led
#include <FastLED.h>

//INFO
const float VER = 0.56; 

//Leds
const int DATA_PIN = 4; //Neo Pixel RGB led
const int NUM_LEDS = 1;
CRGB leds[NUM_LEDS];

//Radio
const int txPin = 10;
const int rxPin = 2; //Interrupt 0 ==> Pin 2
const int REPEATS = 2;
const int BITRATE_RADIOHEAD = 2000;
boolean txStatus = false;
boolean rxStatus = false;

const int statusLedPin = 13;
RH_ASK driver(BITRATE_RADIOHEAD, rxPin, txPin, statusLedPin, false);
const int RADIOHEAD_LATENCY_CORRECTION = 50;

void setup() {
  //Serial Setup
  Serial.begin(115200);
  Serial.setTimeout(100);
  Serial.println("{\"ret\":\"Init\"}");
  Serial.println("{\"ret\":\"Test Radio\"}");
  setupLed();
  setupRadio();
  sendInfo();

}

void loop() {
  checkCommand();
}

/*

Main Functions

*/


//Check if a command is valid
void checkCommand() {
  if (Serial.available() > 0)
  {
    //TODO: improve command checking by avoiding String variable
    String line = Serial.readStringUntil('\n');
    String command = getValue(line, '/', 1);
    if (command == "") {
      Serial.println("{\"err\":\"INVALID COMMAND - /info,/radio,/led\"}");
    }
    else
    {
      if (command == "radio") {
        String radioType = getValue(line, '/', 2);
        if (radioType == "old") {
          sendOld434(line);
        }
        else if (radioType == "new") {
          sendNew434(line);
        }
        else if (radioType == "text") {
          sendText434(line);
        }
        else {
          Serial.println("{\"err\":\"INVALID PROTOCOL - /radio/old,/radio/new,/radio/text\"}");
        }
      }
      else if (command == "info") {
        sendInfo();
      }
    }
  }
}

//Send information on the Arduino
void sendInfo() {
  Serial.print("{\"file\":\"radio2serial.ino\",\"url\":\"smarturl.it/radio2serial\",\"ver\":\"");
  Serial.print(VER);
  Serial.print("\",\"pins\":\"tx:");
  Serial.print(txPin);
  Serial.print(";rx:");
  Serial.print(rxPin);
  Serial.print(";led:");
  Serial.print(DATA_PIN);
  Serial.print("\",\"state\":\"tx:");
  Serial.print(txStatus);
  Serial.print(";rx:");
  Serial.print(rxStatus);
  Serial.println("\"}");
}


//Equivalent of explode in PHP
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

/*



RADIO


*/

/*
Setup Radio
*/
void setupRadio(){
  //Radio Switch SETUP
  checkRxRadio();
  RemoteReceiver::init(-1, REPEATS, showOld434);
  NewRemoteReceiver::init(-1, REPEATS, showNew434);
  InterruptChain::setMode(0, CHANGE);
  InterruptChain::addInterruptCallback(0, RemoteReceiver::interruptHandler);
  InterruptChain::addInterruptCallback(0, NewRemoteReceiver::interruptHandler);
  InterruptChain::addInterruptCallback(0, showVW434);
  //RadioHead Text SETUP
  driver.init();
  if (rxStatus) {
  checkTxRadio(); 
}
}

/*
Check Radio
*/

void checkRxRadio() {
  int testRadioHigh = 0;
  int testRadioLow = 0;
  for (int i = 0; i < 64; i++) {
    if (digitalRead(rxPin)) {
      testRadioHigh++;
    }
    else
    {
      testRadioLow++;
    }
    delay(25);
  }

  if (testRadioLow == 128) {
    rxStatus = false;
    Serial.println("{\"err\":\"Rx not plugged\"}");
    leds[0] = CRGB::Yellow;

  }
  else
  {
    rxStatus = true;
  }

}

void checkTxRadio() {
  sendNew434("/radio/new/1234/0/off");
  if (!txStatus) {
      Serial.println("{\"err\":\"Tx not plugged\"}");
      leds[0] = CRGB::Red;
      FastLED.show();
  }
}





/*
Display Radio codes
*/

//Show 434 code with no self-learn code
void showOld434(unsigned long receivedCode, unsigned int period) {
  Serial.print("{\"data\" : \"/radio/old/");
  Serial.print(receivedCode);
 
  if(period < 400 || period > 500){
  Serial.print("/");
  Serial.print(period);
  }
  
  Serial.println("\"}");
}

//Show 434 code with self-learn code
// /new434/address/id/level/(period)
void showNew434(NewRemoteCode receivedCode) {
  // Print the received code.
  Serial.print("{\"data\" : \"/radio/new/");
  Serial.print(receivedCode.address);
  Serial.print("/");
  Serial.print(receivedCode.unit);
  Serial.print("/");

  switch (receivedCode.switchType) {
    case NewRemoteCode::off:
      Serial.print("off");
      break;
    case NewRemoteCode::on:
      Serial.print("on");
      break;
    case NewRemoteCode::dim:
      Serial.print(receivedCode.dimLevel);
      break;
  }
  if(receivedCode.period < 190 || receivedCode.period > 310){
  Serial.print("/");
  Serial.print(receivedCode.period);
  }
  Serial.println("\"}");

  if (receivedCode.address == 1234) {
    txStatus = true;
  }

}

void showVW434() {

  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) // Non-blocking
  {
    char* radiocode;
    buf[buflen] = '\0';
    radiocode = (char *)buf;
    Serial.print("{\"data\" : \"/radio/text/");
    Serial.print(radiocode);
    Serial.println("\"}");
  }
}

/*
Send radio code
*/

//Send a text with Radiohead /radio/text/Hello world
void sendText434(String line) {
  String code = getValue(line, '/', 3);
  char msg[code.length() + 1];
  //Serial.println(code.length());
  if(code.length() < 60){
  code.toCharArray(msg, code.length() + 1);
  driver.send((uint8_t *)msg, strlen(msg));
  //driver.send((uint8_t *)msg, strlen(msg));
  driver.waitPacketSent();
  Serial.print("{\"data\" : \"/radio/text/");
  Serial.print(msg);
  Serial.println("\"}");
  Serial.println("{\"ret\":\"/radio/OK\"}");
  blinkLed(CRGB::Yellow);

  }
  else
  {
  Serial.println("{\"err\":\"Text too long\"}");
  leds[0] = CRGB::Blue;
  FastLED.show();
  }
  
}


// Send an old radio code
// /radio/old/code/(period)
void sendOld434(String line) {
  String code_temp = getValue(line, '/', 3);
  String period_temp = getValue(line, '/', 4);

  long code = code_temp.toInt();
  int period = period_temp.toInt();

  if (period == 0) {
    period = 400;
  }

  RemoteTransmitter::sendCode(txPin, code, (period - RADIOHEAD_LATENCY_CORRECTION), 3);
  Serial.println("{\"ret\":\"/radio/OK\"}");
  
  //Serial.println(code);
  //Serial.println(period);
}

// Send a new radiocode
// /new/address/id/level/(period)
void sendNew434(String line) {
  String address_temp = getValue(line, '/', 3);
  String id_temp = getValue(line, '/', 4);
  String level_temp = getValue(line, '/', 5);
  String period_temp = getValue(line, '/', 6);

  //Check if we want to dim or not
  int level;
  if (level_temp == "on") {
    level = 16;
  }
  else if (level_temp == "off") {
    level = 0;
  }
  else {
    level = level_temp.toInt();
  }

  //If period was not set or badly set we set it to 260 (default)
  int period = period_temp.toInt();

  //Default period
  if (period == 0) {
    period = 200;
  }


  int id = id_temp.toInt();
  long address = address_temp.toInt();


  NewRemoteTransmitter transmitter(address, txPin, (period - RADIOHEAD_LATENCY_CORRECTION), 3);
  if (level == 0) {
    transmitter.sendUnit(id, false);
  }
  else if (level >= 1 && level <= 15) {
    transmitter.sendDim(id, level);
  }
  else if (level == 16) {
    transmitter.sendUnit(id, true);
  }

  Serial.println("{\"ret\":\"/radio/OK\"}");
  blinkLed(CRGB::Green);
  //Serial.println(address);
  //Serial.println(id);
  //Serial.println(level);
  //Serial.println(period);
}

/*




Neo Pixel Led 



*/


void setupLed(){
 //Led setup
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  blinkLed(CRGB::Yellow);
}

void controlLed(String line) {
  String color = getValue(line, '/', 2);
  if (color == "red") {
    blinkLed(CRGB::Red);
  }
  else if (color == "green") {
    blinkLed(CRGB::Green);
  }
  else if (color == "blue") {
    blinkLed(CRGB::Blue);
  }
  else if (color == "white") {
    blinkLed(CRGB::White);
  }
  else if (color == "off") {
    blinkLed(CRGB::Black);
  }
  else {
    Serial.println("{\"err\":\"INVALID COLOR : red,green,white,off\"}");
  }
}

void blinkLed(const CRGB& color) {
  // Turn the LED on, then pause
  for (int i = 0; i <= 2; i++) {
    leds[0] = color;
    FastLED.show();
    delay(50);
    // Now turn the LED off, then pause
    leds[0] = CRGB::Black;
    FastLED.show();
    delay(50);
  }
}



