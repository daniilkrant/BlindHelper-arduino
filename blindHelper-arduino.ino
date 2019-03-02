#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

#include "Ticker.h"

#define BUZZER_PIN 16
#define TONE_CHANNEL 0

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
int paramArrDef[] = {220,200,300,5,1000,5};
bool isParamDef = true;
bool state = false;
volatile int count = 0;
Ticker ticker;

const char* ssid     = "BlindHelper000";
const char* password = "12431243";

WiFiServer server(80);

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

void buzz(){
     state = true;
}

void setup() {
  ledcSetup(BUZZER_PIN, 5000, 8);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");

  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();

  Serial.println("Server started");
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
  WiFiClient client = server.available();

  if (client) {     
    Serial.println("New Client.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.indexOf("GET //") >= 0) {
            currentLine.remove(0, currentLine.indexOf("GET /") + 5);
            currentLine.remove(currentLine.indexOf("& "), currentLine.length());
            Serial.println(currentLine);
            paramToDef();
            parsLineToInt(currentLine);
            buzzing();
          }
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("Client Disconnected.");
  }
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
    ticker.attach(paramArr[0]/1000, buzz);
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

  ticker.detach();
  state = false;
}
