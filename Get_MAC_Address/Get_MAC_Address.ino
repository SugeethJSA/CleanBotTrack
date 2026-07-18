#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);
  
  Serial.println("\n--------------------------------");
  Serial.println("ESP32 MAC ADDRESS DISCOVERY");
  Serial.println("--------------------------------");
  
  // Print the MAC address
  Serial.print("Your MAC Address is: ");
  Serial.println(WiFi.macAddress());
  
  Serial.println("--------------------------------");
  Serial.println("Please copy this and save it!");
}

void loop() {
  // Do nothing
}
