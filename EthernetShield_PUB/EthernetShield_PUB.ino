/*
 * M6E to Ethernet Shield Arduino.
 *
 */

#include <SPI.h>
#include <Ethernet2.h>
#include <SoftwareSerial.h>
#include <PubNub.h>

byte mac[] = {0x90,0xa2,0xda,0x10,0x10,0x4e};
IPAddress ip(192, 168, 1, 177);

void setup() {  
  Serial.begin(115200);
  Serial.println(F("Serial set up"));

  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));
    Ethernet.begin(mac, ip);
  }
  delay(1000);
  
  Serial.println(F("Ethernet set up"));

  PubNub.begin("pub-c-85b55b4b-9005-4dd2-88aa-6c8bc7c65066", "sub-c-d8faa678-10a1-11e6-a8fd-02ee2ddab7fe");
  Serial.println(F("PubNub set up"));
}

void loop() {
  Ethernet.maintain();
  
  EthernetClient *client;
  char pubmsg[64] = "{\"card\":[\"AB CD EF 12 34 56 78 9A 11\"]}"; 
  
  Serial.print(F("publishing message: "));
  Serial.println(pubmsg);
  client = PubNub.publish("mindreader", pubmsg);
  if (!client) {
    Serial.println(F("publishing error"));
  } else {
    client->stop();
  }
  Serial.print(F("Memory left: "));
  Serial.println(free_ram());
  delay(1000);
}

int free_ram() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

