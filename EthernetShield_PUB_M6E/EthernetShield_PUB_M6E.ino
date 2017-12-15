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
#define SLEEP 2

byte mac[] = {0x90,0xa2,0xda,0x10,0x10,0x4e};
IPAddress ip(192, 168, 1, 177);

SoftwareSerial softSerial(2, 3); //RX, TX
RFID nano; //Create instance

int sleepState = LOW;

struct Card {
  byte epc[12];
  unsigned long rTime;
  bool pbSent;
  Card *next;
  Card *prev;
};

Card *head;
Card *tail;

byte cardCount = 0;
unsigned long now;
bool cardDetect;

void setup() {  
  Serial.begin(115200);
  Serial.println(F("Serial set up"));
  
  pinMode(GLED, OUTPUT);
  pinMode(RLED, OUTPUT);
  pinMode(M6E_ENABLE, OUTPUT);
  //pinMode(SLEEP, INPUT);              Sleep code
  //digitalWrite(SLEEP, HIGH);
  //attachInterrupt(0, sleepNow, HIGH);
  blinkLED(GLED);
  blinkLED(RLED);

  now = millis();
  cardDetect = false;

  digitalWrite(M6E_ENABLE, HIGH); //Turns on M6E does nothing if pin is not connected

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

  /** Sleep code to be tested.
  sleepState = digitalRead(SLEEP);
  if (sleepState == HIGH) {
     digitalWrite(M6E_ENABLE, LOW); //Turns off M6E
     sleep_enable();
     attachInterrupt(0, SLEEP, LOW)
  }
  **/
  unsigned long snap = millis();
  if ((snap - now) > 3000) {
    nano.stopReading();
    now = snap;
    delOldCards();
    if (cardDetect) pubSend();
    delay(1000);
    nano.startReading();  
  }
  
  if (nano.check() == true) {
    
    byte responseType = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

    if (responseType == RESPONSE_IS_KEEPALIVE)
    {
      Serial.println(F("Scanning"));
    }
    else if (responseType == RESPONSE_IS_TAGFOUND)
    {
      digitalWrite(GLED, HIGH);    

      byte tagEPCBytes = min(12, nano.getTagEPCBytes()); //Get the number of bytes of EPC from response
      byte cardBytes[12];

      for (byte x = 0 ; x < tagEPCBytes ; x++)
      {
        cardBytes [x] = nano.msg[31 + x];
      }

      //sendCardNow(cardBytes); For sending individual cards
      
      
      if (newCard(cardBytes, tagEPCBytes)) {
        cardDetect = true;
      }
      

    }
    else if (responseType == ERROR_CORRUPT_RESPONSE)
    {
      Serial.println(F("Bad CRC"));
    }
    else
    {
      //Unknown response
      Serial.print(F("Unknown error"));
    }
  }
}

/** Sleep code to be tested
void wakeUp() {
  sleep_disable
  detachInterrupt(0);
  sleepState = 0;
}

void sleepNow() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(0, wakeUp, LOW);
  sleep_mode();
  sleep_disable();
  detachInterrupt(0);
}
**/

void sendCardNow(byte c[]) {
  EthernetClient *client;
  char pubmsg[128] = "{\"card\":[\"";
  for (byte x = 0 ; x < 12 ; x++)
      {
        if (c[x] < 0x10) {
          sprintf(pubmsg + strlen(pubmsg), "0%X ",c[x]);
        }
        else {
          sprintf(pubmsg + strlen(pubmsg), "%X ", c[x]);
        }
      }
  pubmsg[strlen(pubmsg) - 1] = '\0'; //gets rid of extra space
  strcat(pubmsg, "]}");
  Serial.print(F("publishing message: "));
  Serial.println(pubmsg);
  client = PubNub.publish("mindreader", pubmsg);
  digitalWrite(GLED, LOW);
  if (!client) {
    blinkLED(RLED);
    Serial.println(F("publishing error"));
  } else {
    client->stop();
  }
}

void pubSend() {
  EthernetClient *client;
  char pubmsg[128] = "{\"cards\":[\"";

  Card *current = head;
  while (current) {
    if (!current->pbSent) {
      for (byte x = 0 ; x < 12 ; x++)
      {
        if (current->epc[x] < 0x10) {
          sprintf(pubmsg + strlen(pubmsg), "0%X ",current->epc[x]);
        }
        else {
          sprintf(pubmsg + strlen(pubmsg), "%X ", current->epc[x]);
        }
      }
      pubmsg[strlen(pubmsg) - 1] = '\0';
      strcat(pubmsg, "\", \"");
    }
    current = current->next;
  }
  pubmsg[strlen(pubmsg) - 3] = '\0';
  strcat(pubmsg, "]}");

  Serial.print(F("publishing message: "));
  Serial.println(pubmsg);
  client = PubNub.publish("mindreader", pubmsg);
  Serial.print(F("Memory left: "));
  Serial.println(free_ram());
  digitalWrite(GLED, LOW);
  if (!client) {
    blinkLED(RLED);
    Serial.println(F("publishing error"));
  } else {
    current = head;
    Serial.print(F("Cards in memory: "));
    while (current) {
      for (int i = 0; i < 12; i++) {
        Serial.print(" ");
        Serial.print((int)current->epc[i]);
      }      
      current->pbSent = true;
      current = current->next;
      if (current) Serial.print(F(","));
    }
    Serial.println();
    cardDetect = false;
    client->stop();
  }
}

void delOldCards() {  
  Card *current = tail;
  while (current) {
    if ((now - current->rTime) > 20000) {
      tail = current->prev;      
      delete current;
      if (cardCount > 1) tail->next = NULL;
      else {
          delete head;
          head = NULL;
      }
      current = tail;
      cardCount--;
    } else {
      break;
    }
  }
}

bool newCard(byte c[], byte len) {
  Card *current = head;
  while (current) {
    if (aEqual(c, current->epc, len)) return false;
    current = current->next;
  }
  current = new Card();
  for (byte i = 0; i < len; i++) {
    current->epc[i] = c[i];
  }
  current->prev = NULL;
  current->rTime = millis();
  current->next = head;
  current->pbSent = false;
  if (head) head->prev = current;
  head = current;
  if (cardCount == 0) tail = head;
  cardCount++;
  return true;
}

bool aEqual(byte a[], byte b[], int len) {
  for (byte i = 0; i < len; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
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

int free_ram() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
