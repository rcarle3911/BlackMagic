/*
 * M6E to ESP final program. Communicates with PubNub Channel
 *
 */
#include <SoftwareSerial.h>
#include <Adafruit_ESP8266.h>
#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module

#define ESP_RX   7
#define ESP_TX   6
#define ESP_RST  5
#define M6E_RX   2
#define M6E_TX   3

#define ESP_SSID "O2MobileWiFi_3CE5" // Your network name here
#define ESP_PASS "10657226" // Your network password here

#define HOST     "www.pubnub.com"     // Host to contact
#define PORT     80                     // 80 = HTTP default port

SoftwareSerial espSerial(ESP_RX, ESP_TX);  //ESP software serial
SoftwareSerial m6eSerial(M6E_RX, M6E_TX);

// Must declare output stream before Adafruit_ESP8266 constructor; can be
// a SoftwareSerial stream, or Serial/Serial1/etc. for UART.
// Must call begin() on the stream(s) before using Adafruit_ESP8266 object.
Adafruit_ESP8266 wifi(&espSerial, &Serial, ESP_RST);
RFID nano; //Create instance

void setup()
{  
  Serial.begin(115200);
  while (!Serial);
  Serial.println(F("Initializing..."));

  //Setup Nano
  if (setupNano(38400) == false) //Configure nano to run at 38400bps
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }

  nano.setRegion(REGION_EUROPE); //Set to North America

  nano.setReadPower(500); //5.00 dBm. Higher values may cause USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  Serial.println(F("M6E Set up"));

  Serial.println(F("Setting up WiFi"));
  setupWiFi();

  nano.startReading();
}

void loop()
{
  if (nano.check() == true) {
    byte responseType = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

    if (responseType == RESPONSE_IS_KEEPALIVE)
    {
      Serial.println(F("Scanning"));
    }
    else if (responseType == RESPONSE_IS_TAGFOUND)
    {
      // put your main code here, to run repeatedly:  
      char* page = "/publish/pub-c-85b55b4b-9005-4dd2-88aa-6c8bc7c65066/sub-c-d8faa678-10a1-11e6-a8fd-02ee2ddab7fe/0/mindreader/0/{\"card\":[\"";
      byte tagEPCBytes = nano.getTagEPCBytes(); //Get the number of bytes of EPC from response

      for (byte x = 0; x < tagEPCBytes; x++) {
        if (nano.msg[31 + x] < 0x10) sprintf(page + strlen(page), "0%X ", nano.msg[31 + x]);
        else sprintf(page + strlen(page), "%X ", nano.msg[31 + x]);
      }      
      sprintf(page + strlen(page) - 1, "\"]}");
      
      Serial.print(F("publishing message: "));
      Serial.println(page);
      
      if(wifi.connectTCP(F(HOST), PORT)) {
      Serial.print(F("OK\nRequesting page..."));
      if(wifi.requestURL(page)) {
        Serial.println(F("OK\nSearching for string..."));
        // Search for a phrase in the open stream.
        // Must be a flash-resident string (F()).
        if(wifi.find(F("working"), true)) {
          Serial.println(F("found!"));
        } else {
          Serial.println(F("not found."));
        }
      } else { // URL request failed
        Serial.println(F("error"));
      }
      wifi.closeTCP();
      } else { // TCP connect failed
        Serial.println(F("D'oh!"));
      }
    }
  }
}
//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.begin(m6eSerial); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  m6eSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while(!m6eSerial); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while(m6eSerial.available()) m6eSerial.read();
  
  nano.getVersion();

  if (nano.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
  {
    //This happens if the baud rate is correct but the module is doing a ccontinuous read
    nano.stopReading();

    Serial.println(F("Module continuously reading. Asking it to stop..."));

    delay(1500);
  }
  else
  {
    //The module did not respond so assume it's just been powered on and communicating at 115200bps
    m6eSerial.begin(115200); //Start software serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    m6eSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
  }

  Serial.println(F("Checking Version..."));
  //Test the connection 
  nano.getVersion();
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right
  
  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock
}

void setupWiFi() {
  char buffer[50];

  // This might work with other firmware versions (no guarantees)
  // by providing a string to ID the tail end of the boot message:
  
  // comment/replace this if you are using something other than v 0.9.2.4!
  //wifi.setBootMarker(F("Version:0.9.2.4]\r\n\r\nready"));

  espSerial.begin(9600); // Soft serial connection to ESP8266

  Serial.println(F("Adafruit ESP8266 Demo"));

  // Test if module is ready
  Serial.print(F("Hard reset..."));
  if(!wifi.hardReset()) {
    Serial.println(F("no response from module."));
    while(1) delay(500);
  }
  Serial.println(F("OK."));

  Serial.print(F("Soft reset..."));
  if(!wifi.softReset()) {
    Serial.println(F("no response from module."));
    while(1) delay(500);
  }
  Serial.println(F("OK."));

  Serial.print(F("Checking firmware version..."));
  wifi.println(F("AT+GMR"));
  if(wifi.readLine(buffer, sizeof(buffer))) {
    Serial.println(buffer);
    wifi.find(); // Discard the 'OK' that follows
  } else {
    Serial.println(F("error"));
  }

  Serial.print(F("Connecting to WiFi..."));
  if(wifi.connectToAP(F(ESP_SSID), F(ESP_PASS))) {

    // IP addr check isn't part of library yet, but
    // we can manually request and place in a string.
    Serial.print(F("OK\nChecking IP addr..."));
    wifi.println(F("AT+CIFSR"));
    if(wifi.readLine(buffer, sizeof(buffer))) {
      Serial.println(buffer);
      wifi.find(); // Discard the 'OK' that follows
    } else { // IP addr check failed
      Serial.println(F("error"));
    }
  } else { // WiFi connection failed
    Serial.println(F("FAIL"));
  }
}
