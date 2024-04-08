// /*
//   Rui Santos
//   Complete project details at https://RandomNerdTutorials.com/esp-now-two-way-communication-esp32/
  
//   Permission is hereby granted, free of charge, to any person obtaining a copy
//   of this software and associated documentation files.
  
//   The above copyright notice and this permission notice shall be included in all
//   copies or substantial portions of the Software.
// */


#include <esp_now.h>
#include <WiFi.h>
#include <M5StickCPlus.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// MAC of sticks to send data to
uint8_t broadcastAddress[][6] = {
  {0x4C, 0x75, 0x25, 0xCB, 0x7F, 0x50}, // 28
  {0x4C, 0x75, 0x25, 0xCB, 0x94, 0xAC}  // 17
};

// Define variables to store M5Stick readings to be sent
float temperature;
float humidity;

// Define variables to store incoming readings
float incomingTemp;
float incomingHumidity;

// Variable to store if sending data was successful
String success;

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
    float temp;
    float humidity;
} struct_message;

// Create a struct_message called SensorReadings to hold sensor readings
struct_message SensorReadings;

// Get the count of the array broadcastAddress
int numberOfSticks = sizeof(broadcastAddress) / sizeof(broadcastAddress[0]);

// Buffer to store the formatted temperature and humidity string
char dataString[100]; // Adjust the size as needed based on your formatting requirements

// Declare a pointer to hold incoming sensor readings
struct_message* incomingReadings;

esp_now_peer_info_t peerInfo;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status == ESP_NOW_SEND_SUCCESS) {
    success = "Delivery Success :)";
  }
  else {
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

  // Init Serial2 to send data to uno
  Serial2.begin(9600, SERIAL_8N1, 0, 26);

  M5.begin(true, true, true);

  int x = M5.IMU.Init(); // Return 0 is OK, return -1 is unknown
  if (x != 0) {
    Serial.println("IMU initialisation fail!");  
  }

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
  // get the status of transmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register all peers
  for (int i = 0; i < numberOfSticks; i++) {
    memcpy(peerInfo.peer_addr, broadcastAddress[i], 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
      return;
    } else {
      Serial.print("M5Stick ");
      Serial.print(i + 1);
      Serial.println(" is registered.");
    }
  }

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
}

// Function to read temperature from M5Stick
float readTemperature() {
  float t;
  M5.IMU.getTempData(&t);
  t = (t - 32.0) * 5.0 / 9.0;
  return t;
} 

// Function to read humidity from M5Stick
float readHumidity() {
  // Simulate a humidity reading, replace this with actual code to read humidity
  return random(20,100); // Random humidity value in the range of 0-50
}

void loop() {
  getReadings();
 
  // Set values to send
  SensorReadings.temp = temperature;
  SensorReadings.humidity = humidity;

  // Send message via ESP-NOW to each M5Stick
  for (int i = 0; i < sizeof(broadcastAddress) / sizeof(broadcastAddress[0]); i++) {
    Serial.print("Sending data to M5Stick ");
    Serial.println(i + 1);

    Serial.print("Data sent - T: ");
    Serial.print(SensorReadings.temp);
    Serial.print(", H: ");
    Serial.println(SensorReadings.humidity);

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

// Function to get readings from M5Stick
void getReadings() {
  temperature = readTemperature();
  humidity = readHumidity();
}

// Function to display the data on LCD
void updateDisplay() {
  for (int i = 0; i < numberOfSticks; i++) {
    // Get the MAC address of the current M5Stick
    String macAddress = macToString(broadcastAddress[i]);

    // Format the data into the dataString buffer
    snprintf(dataString, sizeof(dataString), "M %s - T: %2.1f C, H: %2.1f%\n", macAddress.c_str(), incomingReadings[i].temp, incomingReadings[i].humidity);

    // Print the formatted string
    Serial.println(dataString);

    // Display the formatted string on M5Stick LCD
    M5.Lcd.setCursor(0, 20 + i * 40, 2);
    M5.Lcd.printf("M %d - T: %2.1f C, H: %2.1f", i + 1, incomingReadings[i].temp, incomingReadings[i].humidity);

    // Send the formatted string over Serial2 to Uno
    Serial2.write(dataString);
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
