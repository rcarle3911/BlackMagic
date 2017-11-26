/*
 * M6E to Ethernet Shield Arduino.
 *
 */

#include <SPI.h>
#include <Ethernet2.h>
#include <SoftwareSerial.h>
#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
#include <PubNub.h>

#define GLED 7
#define RLED 6
#define M6E_ENABLE 8

byte mac[] = {0x90,0xa2,0xda,0x10,0x10,0x4e};
IPAddress ip(192, 168, 1, 177);

SoftwareSerial softSerial(2, 3); //RX, TX
RFID nano; //Create instance

void setup() {  
  Serial.begin(115200);
  Serial.println(F("Serial set up"));
  
  pinMode(GLED, OUTPUT);
  pinMode(RLED, OUTPUT);
  pinMode(M6E_ENABLE, OUTPUT);

  blinkLED(GLED);
  blinkLED(RLED);

  digitalWrite(M6E_ENABLE, HIGH); //Turns on M6E

  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));
    Ethernet.begin(mac, ip);
  }
  delay(1000);
  
  Serial.println(F("Ethernet set up"));

  PubNub.begin("pub-c-85b55b4b-9005-4dd2-88aa-6c8bc7c65066", "sub-c-d8faa678-10a1-11e6-a8fd-02ee2ddab7fe");
  Serial.println(F("PubNub set up"));

  Serial.println(F("Setting up M6E"));
  if (setupNano(38400) == false) //Configure nano to run at 38400bps
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }
    
  nano.setRegion(REGION_EUROPE); //Set to Europe

  nano.setReadPower(500); //5.00 dBm. Higher values may cause USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling
  
  Serial.println(F("M6E set up"));

  nano.startReading();
}

void loop() {
  Ethernet.maintain();

  if (nano.check() == true) {
    
    byte responseType = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

    if (responseType == RESPONSE_IS_KEEPALIVE)
    {
      Serial.println(F("Scanning"));
    }
    else if (responseType == RESPONSE_IS_TAGFOUND)
    {
      solidLED(GLED);
      EthernetClient *client;
      char pubmsg[64] = "{\"card\":[\"";

      byte tagEPCBytes = nano.getTagEPCBytes(); //Get the number of bytes of EPC from response

      //Print EPC bytes, this is a subsection of bytes from the response/msg array

      for (byte x = 0 ; x < tagEPCBytes ; x++)
      {
        if (nano.msg[31 + x] < 0x10) sprintf(pubmsg + strlen(pubmsg), "0%X ", nano.msg[31 + x]);
        else sprintf(pubmsg + strlen(pubmsg), "%X ", nano.msg[31 + x]);
      }
      strcat(pubmsg, "\"]}"); 

      Serial.print(F("publishing message: "));
      Serial.println(pubmsg);
      client = PubNub.publish("mindreader", pubmsg);
      if (!client) {
        blinkLED(RLED);
        Serial.println(F("publishing error"));
      } else {
        blinkLED(GLED);
        client->stop();
      }
    }
    else if (responseType == ERROR_CORRUPT_RESPONSE)
    {
      Serial.println(F("Bad CRC"));
      solidLED(RLED);
    }
    else
    {
      //Unknown response
      Serial.print(F("Unknown error"));
    }
  }
}

void blinkLED(int pin) {
  for (int i = 0; i < 5; i++) {
      digitalWrite(pin, HIGH);
      delay(100);
      digitalWrite(pin, LOW);
      delay(100);
  }
}

void solidLED(int pin) {
  digitalWrite(pin, HIGH);
  delay(1000);
  digitalWrite(pin, LOW);
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.begin(softSerial); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while(!softSerial); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while(softSerial.available()) softSerial.read();
  
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
    softSerial.begin(115200); //Start software serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    softSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
  }

  //Test the connection
  nano.getVersion();
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock
}
