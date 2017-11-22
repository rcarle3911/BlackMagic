/*
 * M6E to ESP final program. Communicates with PubNub Channel
 *
 */
#include <ESP8266WiFi.h>
#define PubNub_BASE_CLIENT WiFiClient
#define PUBNUB_DEFINE_STRSPN_AND_STRNCASECMP
#include <PubNub.h>
#include <Wire.h>

#define ESP_SSID "O2MobileWiFi_3CE5" // Your network name here
#define ESP_PASS "10657226" // Your network password here

#define PUBKEY "pub-c-85b55b4b-9005-4dd2-88aa-6c8bc7c65066"
#define SUBKEY "sub-c-d8faa678-10a1-11e6-a8fd-02ee2ddab7fe"
#define CHANNEL "mindreader"
#define WAKE_SENSE 10

const int MPU_addr=0x68;
uint16_t prevAcX, prevAcY, prevAcZ, prevTmp, prevGyX, prevGyY, prevGyZ;
uint16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

// Must declare output stream before Adafruit_ESP8266 constructor; can be
// a SoftwareSerial stream, or Serial/Serial1/etc. for UART.
// Must call begin() on the stream(s) before using Adafruit_ESP8266 object.

void setup()
{  
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0);   // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  
  Serial.begin(115200);
  while (!Serial);
  Serial.println(F("Initializing..."));
  
  WiFi.begin(ESP_SSID, ESP_PASS);

  Serial.print(F("Connecting"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();

  Serial.print(F("Connected, IP address: "));
  Serial.println(WiFi.localIP());

  Serial.print(F("Connecting to PubNub"));
  PubNub.begin(PUBKEY, SUBKEY);
}

void loop()
{
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true); //Request 14 registers

  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  Serial.print("AcX = "); Serial.print(AcX);
  Serial.print(" | AcY = "); Serial.print(AcY);
  Serial.print(" | AcZ = "); Serial.print(AcZ);
  Serial.print(" | Tmp = "); Serial.print(Tmp/340.00+36.53);  //equation for temperature in degrees C from datasheet
  Serial.print(" | GyX = "); Serial.print(GyX);
  Serial.print(" | GyY = "); Serial.print(GyY);
  Serial.print(" | GyZ = "); Serial.println(GyZ);

  if (  abs(AcX - prevAcX) > WAKE_SENSE ||
        abs(AcY - prevAcY) > WAKE_SENSE ||
        abs(AcZ - prevAcZ) > WAKE_SENSE) {
    String msg = "{\"gyro\": [\"" + String(AcX) + "\", \"" +
                                    String(AcY) + "\", \"" +
                                    String(AcZ) + "\", \"" +
                                    String(Tmp) + "\", \"" +
                                    String(GyX) + "\", \"" +
                                    String(GyY) + "\", \"" +
                                    String(GyZ) + "\"]}";

    char buf[64];
    Serial.print(F("Publishing message: "));
    Serial.println(msg);
    msg.toCharArray(buf, 64);
    WiFiClient *client = PubNub.publish(CHANNEL, buf);
    if (client) client->stop();
  } else {
    ESP.deepSleep(3e6); //Sleep for 3 second
  }
}
