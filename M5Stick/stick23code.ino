/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp-now-two-way-communication-esp32/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <esp_now.h>
#include <WiFi.h>
#include <M5StickCPlus.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


//MAC of sticks to send data to
uint8_t broadcastAddress[][6] = {
  {0x4C,0x75,0x25,0xCB,0x7F,0x50}, //28
  {0x4C,0x75,0x25,0xCB,0x94,0xAC}  //17
};

// Define variables to store m5stick readings to be sent
float temperature;

// Define variables to store incoming readings
float incomingTemp;

// Variable to store if sending data was successful
String success;

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
    float temp;
} struct_message;

// Create a struct_message called BME280Readings to hold sensor readings
struct_message SensorReadings;


// Get the count of the array broadcastAddress
int numberOfSticks = sizeof(broadcastAddress) / sizeof(broadcastAddress[0]);

// Buffer to store the formatted temperature string
char tempString[50]; // Adjust the size as needed based on your formatting requirements

// Declare a pointer to hold incoming sensor readings
struct_message* incomingReadings;

esp_now_peer_info_t peerInfo;


// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Fail :(";
  }
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  // Determine which M5Stick sent the data
  int stickIndex = -1;
  for (int i = 0; i < numberOfSticks; i++) {
    if (memcmp(mac, broadcastAddress[i], 6) == 0) {
      stickIndex = i;
      break;
    }
  }
  
  // If the MAC address matches one of the M5Sticks, store the data in the corresponding slot
  if (stickIndex != -1) {
    memcpy(&incomingReadings[stickIndex], incomingData, sizeof(struct_message));
    Serial.print("Data received from M5Stick ");
    Serial.println(stickIndex + 1);
  }
}
 
void setup() {

    // Allocate memory for incomingReadings based on the number of sticks
  incomingReadings = new struct_message[numberOfSticks];

  // Init Serial Monitor
  Serial.begin(115200);

  //init serial2 to send data to uno
  Serial2.begin(9600, SERIAL_8N1, 0, 26);

  M5.begin(true,true,true);

  int x = M5.IMU.Init(); //return 0 is ok, return -1 is unknown
  if(x!=0)
    Serial.println("IMU initialisation fail!");  

  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.printf("Starting", 0);

 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  

  // Register all peers
  for (int i = 0; i < numberOfSticks; i++) {
    memcpy(peerInfo.peer_addr, broadcastAddress[i], 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
      return;
    }else {
      Serial.print("M5Stick ");
      Serial.print(i + 1);
      Serial.println(" is registered.");
    }
  }


  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
}

//gets the temp from m5stick
float readTemperature(){
  float t;
  M5.IMU.getTempData(&t);
  t = (t-32.0)*5.0/9.0;
  return t;
} 
 
void loop() {
  getReadings();
 
  // Set values to send
  SensorReadings.temp = temperature;


    // Send message via ESP-NOW to each M5Stick
  for (int i = 0; i < sizeof(broadcastAddress) / sizeof(broadcastAddress[0]); i++) {
    Serial.print("Sending data to M5Stick ");
    Serial.println(i + 1);

    Serial.print("Data sent: ");
    Serial.println(SensorReadings.temp); // Assuming you want to print the temperature

    esp_err_t result = esp_now_send(broadcastAddress[i], (uint8_t *)&SensorReadings, sizeof(SensorReadings));
    
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    } else {
      Serial.println("Error sending the data");
    }
  }

  updateDisplay();
  delay(5000);
}

//get readings from m5stick
void getReadings(){
  temperature = readTemperature();
}

//display the data on lcd
void updateDisplay(){

  for (int i = 0; i < numberOfSticks; i++) {
    // Get the MAC address of the current M5Stick
    String macAddress = macToString(broadcastAddress[i]);

    // Format the temperature value into the tempString buffer
    snprintf(tempString, sizeof(tempString), "MAC: %s Temp: %2.1f C\n", macAddress.c_str(), incomingReadings[i].temp);
    Serial.println(macAddress);
    // Print the formatted string
    M5.Lcd.setCursor(0, 20 + i * 40, 2);
    M5.Lcd.printf("M5Stick %d - Temperature: %2.1f ºC", i + 1, incomingReadings[i].temp);

    // Send the formatted temperature string over Serial2 to uno
    Serial2.write(tempString);
  }

}


// Function to convert MAC address to String
String macToString(const uint8_t* macAddress) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(macAddress[i], HEX);
    if (i < 5) {
      result += ':';
    }
  }
  return result;
}



