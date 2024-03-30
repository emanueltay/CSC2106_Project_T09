#include <SoftwareSerial.h>

String sendMessage;
String receivedMessage = "";
SoftwareSerial mySerial(8,9);
void setup() {
  Serial.begin(9600);    // Initialize the Serial monitor for debugging
  while (!Serial) {
  }

  Serial.println("Serial started!");
  mySerial.begin(9600);

}

  void loop() {
  if (mySerial.available() > 0) {
    char receivedChar = mySerial.read();
    if (receivedChar == '\0') {
      Serial.println("empty");
    } else {
      
      // Serial.println(receivedChar);
      receivedMessage += receivedChar;

      uint8_t mydata[33]; // Create a byte array to hold the converted string

      // Convert the string to a byte array
      for (int i = 0; i < 33; i++) {
        mydata[i] = receivedMessage[i];
      }

      // char myString[] = "MAC: 4c:75:25:cb:7f:50 Temp: -0.0";
      // uint8_t mydata[sizeof(myString)]; // Create a byte array to hold the converted string

      // // Convert the string to a byte array
      // for (int i = 0; i < sizeof(myString); i++) {
      //   mydata[i] = myString[i];
      // }

      // receivedMessage += "testing";
      // static uint8_t mydata[] = "";
      
      // String message = "1234";
      // static uint8_t newmessage = atoi (message.c_str());

      // // mydata += newmessage;
      // uint8_t temperature = random(0, 100); // Random temperature between 0 and 100
      // static uint8_t testing = atoi (receivedMessage.c_str());
      
      if (receivedChar == '\n') {

        for (int i = 0; i < 33; i++) {
          Serial.print((char)mydata[i]);
        }

        // for (int i = 0; i < sizeof(mydata); i++) {
        //   Serial.print((char)mydata[i]);
        // }

        Serial.println(receivedMessage);
        // Serial.println(sizeof(receivedChar));

        // Serial.println(newmessage);
        // for (size_t i = 0; i < sizeof(newmessage); ++i) {
        //   Serial.print(newmessage[i]);
        // }

        // Process received message here if needed
        receivedMessage = ""; // Reset the received message
      }
    }
  }
}



