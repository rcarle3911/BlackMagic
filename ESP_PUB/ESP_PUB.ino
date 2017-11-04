#include <Adafruit_ESP8266.h>
#include <SoftwareSerial.h>

#define ESP_RX   7
#define ESP_TX   6
#define ESP_RST  5

#define ESP_SSID "O2MobileWiFi_3CE5" // Your network name here
#define ESP_PASS "10657226" // Your network password here

#define HOST     "www.pubsub.pubnub.com"     // Host to contact

#define PORT     80                     // 80 = HTTP default port


SoftwareSerial softser(ESP_RX, ESP_TX);

Adafruit_ESP8266 wifi(&softser, &Serial, ESP_RST);
// Must call begin() on the stream(s) before using Adafruit_ESP8266 object.

const char pubkey[] = "pub-c-85b55b4b-9005-4dd2-88aa-6c8bc7c65066";
const char subkey[] = "sub-c-d8faa678-10a1-11e6-a8fd-02ee2ddab7fe";
const char channel[] = "mindreader";

void setup() {
  // put your setup code here, to run once:  
  Serial.begin(9600);
  Serial.println(F("Serial set up"));

  Serial.println(F("Setting up WiFi"));
  setupWiFi();
}

void loop() {
  // put your main code here, to run repeatedly:  
  char* page = "/publish/pub-c-85b55b4b-9005-4dd2-88aa-6c8bc7c65066/sub-c-d8faa678-10a1-11e6-a8fd-02ee2ddab7fe/0/mindreader/0/";
  char* msg = "{\"card\":[\"3F 8E 20 0A\"]}";
  Serial.print(F("publishing message: "));
  Serial.println(msg);
  
  if(wifi.connectTCP(F(HOST), PORT)) {
  Serial.print(F("OK\nRequesting page..."));
  if(wifi.requestURL((strcat(page, msg)))) {
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
  delay(5000);
  
}

void setupWiFi() {
  char buffer[50];

  // This might work with other firmware versions (no guarantees)
  // by providing a string to ID the tail end of the boot message:
  
  // comment/replace this if you are using something other than v 0.9.2.4!
  //wifi.setBootMarker(F("Version:0.9.2.4]\r\n\r\nready"));

  softser.begin(9600); // Soft serial connection to ESP8266

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

