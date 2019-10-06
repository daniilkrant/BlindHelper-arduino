#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <DNSServer.h>

#include "BluetoothSerial.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "pthread.h"

#include <Arduino.h>
void paramToDef();
void buzzing();
int time_counter = -3;
bool beaconFound = false;
#define scanPeriod 20 //seconds
const char* beacon_major_minor = "0001";
bool needsToPlay = false;
//BEACON pass - "oncrea"

#define WIFI_CLIENT 0
#define BT_CLIENT 1

#define BUZZER_PIN 15
#define TONE_CHANNEL 0

#define scanTime 5
/**
 * f - frequency of signal
 * l - length of signal
 * p - pause between Signal
 * n - number of signal in group of signal
 * g - pause between Group
 * c - count of call
 */
//                f,  l,  p,  n,g,   c
int paramArr[] = {220,200,300,5,1000,5};
int minParam[] = {200,50,50,1,100,1};
int maxParam[] = {15000,5000,5000,10,10000,20};
int paramArrDef[] = {220,200,300,5,1000,1};
bool isParamDef = true;
const char* ssid     = "BlindHelper0001";
const char* password = "12431243";
//uint8_t mac[] = {0x36, 0x33, 0x33, 0x33, 0x33, 0x33}; //LAST DIGIT SHOULD BE (DESIRED-1)
const byte DNS_PORT = 53;
IPAddress netMsk(255, 255, 255, 0);
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WiFiServer server(80);

BluetoothSerial SerialBT;
BLEScan* pBLEScan;
const char speakerTimeout = 3; //minutes

//DFMiniMp3<HardwareSerial, Mp3Notify> mp3(Serial2);

void setup() {
//  esp_base_mac_addr_set(mac);
  ledcSetup(BUZZER_PIN, 5000, 8);

  paramToDef();
  paramArr[3] = 1;
  paramArr[5] = 1;
  buzzing();
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");

//  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid, password);
  dnsServer.start(DNS_PORT, "*", apIP);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();

  //Serial.println("Server started");
  
//  mp3.begin();
//  mp3.setVolume(24); // 0 - 30
//  uint16_t count = mp3.getTotalTrackCount();
  //Serial.print("files ");
  //Serial.println(count);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  SerialBT.begin(ssid);
  pthread_t BLEThread;
  pthread_create(&BLEThread, NULL, &scanBLE, NULL);
  needsToPlay = true;
}

void processClient(int client_type, WiFiClient *client_ptr) {
  Serial.printf("New Client: %d.\n", client_type);
  if (client_type == WIFI_CLIENT) {
    String currentLine = "";
    while (client_ptr->connected()) {
      if (client_ptr->available()) {
        char c = client_ptr->read();
        if (c == '\n') {
          if (currentLine.indexOf("GET //") >= 0) {
            currentLine.remove(0, currentLine.indexOf("GET /") + 5);
            currentLine.remove(currentLine.indexOf("& "), currentLine.length());
            //Serial.println(currentLine);
            paramToDef();
            parsLineToInt(currentLine);
            client_ptr->stop();
            Serial.printf("Client Disconnected %d.\n", client_type);
            buzzing();
//            mp3.playMp3FolderTrack(1);
          }
          if (currentLine.length() == 0) {
            client_ptr->println("HTTP/1.1 200 OK");
            client_ptr->println("Content-type:text/html");
            client_ptr->println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
  }
  if (client_type == BT_CLIENT) {
    String currentLine = "";
    while (SerialBT.connected()) {
      if (SerialBT.available()) {
        char c = SerialBT.read();
        Serial.println(c);
        if (c == '\n') {
          if (currentLine.indexOf("GET //") >= 0) {
            currentLine.remove(0, currentLine.indexOf("GET /") + 5);
            currentLine.remove(currentLine.indexOf("& "), currentLine.length());
            //Serial.println(currentLine);
            paramToDef();
            parsLineToInt(currentLine);
            SerialBT.end();
            Serial.printf("Client Disconnected %d.\n", client_type);
            buzzing();
            return;
          }
          if (currentLine.length() != 0) {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    } 
  }
   
}

// Request string looks like: GET //100/150/& HTTP/1.1
/**
 * main loop function
 * wait client connected,
 * read reqest,
 * check correct reading line,
 * parsing and assignment to parameters
 * start buzzing
 */
void loop() {
  dnsServer.processNextRequest();

  WiFiClient wifi_client = server.available();
  bool btClient = SerialBT.available();
  if (wifi_client) {     
     processClient(WIFI_CLIENT, &wifi_client);
  }

  if (btClient) {
    processClient(BT_CLIENT, &wifi_client);
  }
}

void tone(uint8_t pin, unsigned int frequency)
{
    ledcAttachPin(pin, TONE_CHANNEL);
    ledcWriteTone(TONE_CHANNEL, frequency);
}

void noTone(uint8_t pin, uint8_t channel)
{
    ledcDetachPin(pin);
    ledcWrite(TONE_CHANNEL, 0);
}

/**
 * reset parameters to defaults
 */
void paramToDef(){
  int pSize = sizeof(paramArr)/sizeof(int);
  for(int i = 0; i < pSize; i++){
    paramArr[i] = paramArrDef[i];
  }
  isParamDef = true;
}

/**
 * Parsing input String data to int parameters,
 * write it to paramArr and
 * start buzzing
 */
 // Line should be /x/x/x/x/x/x/x/
bool parsLineToInt(String line){
    line.remove(0,1);
    int i = 0; //index of paramArr
    int endIndex = 0; //last index in one part ("***/")

    //parsing Input string to int arr
    while(line.length() > 1 && line[0]!='\n'){
      endIndex = line.indexOf('/');
      int buf = line.substring(0, endIndex).toInt();
      if(buf >= minParam[i] && buf <= maxParam[i]){
        paramArr[i] = buf;
        isParamDef = false;
      }
      line.remove(0, endIndex+1);
      i++;
    }
    Serial.println("Start ticker");
    return true;
}

/**
 * repiats 'n' signal 'c' times
 */
void buzzing(){
  Serial.println("Buzzing");
  Serial.print("freq ");
  Serial.println(paramArr[0]);
  Serial.print("len ");
  Serial.println(paramArr[1]);
  Serial.print("pause ");
  Serial.println(paramArr[2]);
  Serial.print("n ");
  Serial.println(paramArr[3]);
  Serial.print("g ");
  Serial.println(paramArr[4]);
  Serial.print("count ");
  Serial.println(paramArr[5]);

  for (int i = 0; i < paramArr[5]; i++) {
    for(int j = 0; j < paramArr[3]; j++){
      tone(BUZZER_PIN, paramArr[0]);
      delay(paramArr[1]);
      noTone(BUZZER_PIN , TONE_CHANNEL);
      delay(paramArr[2]);
    }
    delay(paramArr[4]);
  }
}

void* scanBLE(void *arg) {
  while (1) {
      Serial.println("Scanning BLE:");
      Serial.printf("Timeout: %d\n", time_counter);
      if (time_counter < 0) {
        time_counter = (60 / scanPeriod) * speakerTimeout + 10;
      }
      BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
      time_counter++;
      if (time_counter >= (60 / scanPeriod) * speakerTimeout) {
       for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice d = foundDevices.getDevice(i);
        if (d.haveManufacturerData()) {
        char *pHex = BLEUtils::buildHexData(nullptr, (uint8_t*)d.getManufacturerData().data(), d.getManufacturerData().length());
        //Serial.println(pHex);
        std::string uuid_major_minor(pHex);
        if (uuid_major_minor.find(beacon_major_minor) != std::string::npos) {
         time_counter = 0;
         paramToDef();
         buzzing();
         needsToPlay = true;
         break;
       }
       }
     }
    }
    delay(scanPeriod * 1000);
  }
}
