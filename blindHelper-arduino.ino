#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

#define BUZZER_PIN 16
#define TONE_CHANNEL 0

const char *ssid = "BlindHelper000";
const char *password = "12431243";

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

void loop() {
  WiFiClient client = server.available();

  if (client) {     
    Serial.println("New Client.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {

          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            client.print("Click <a href=\"/H\">here</a> to turn ON the LED.<br>");
            client.print("Click <a href=\"/L\">here</a> to turn OFF the LED.<br>");

            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }

        if (currentLine.endsWith("GET /H")) {
            tone(BUZZER_PIN, 1000);
            delay(500);
            noTone(BUZZER_PIN, TONE_CHANNEL);
            delay(500);
        }
      }
    }
    client.stop();
    Serial.println("Client Disconnected.");
  }
}
