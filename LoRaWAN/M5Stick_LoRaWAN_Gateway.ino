/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 * Copyright (c) 2018 Terry Moore, MCCI
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello,
 * world!", using frequency and encryption settings matching those of
 * the The Things Network.
 *
 * This uses OTAA (Over-the-air activation), where where a DevEUI and
 * application key is configured, which are used in an over-the-air
 * activation procedure where a DevAddr and session keys are
 * assigned/generated for use with all further communication.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
 * g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
 * violated by this sketch when left running for longer)!

 * To use this sketch, first register your application and device with
 * the things network, to set or generate an AppEUI, DevEUI and AppKey.
 * Multiple devices can use the same AppEUI, but each device has its own
 * DevEUI and AppKey.
 *
 * Do not forget to define the radio type correctly in
 * arduino-lmic/project_config/lmic_project_config.h or from your BOARDS.txt.
 *
 *******************************************************************************/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <SoftwareSerial.h>

//
// For normal use, we require that you edit the sketch to replace FILLMEIN
// with values assigned by the TTN console. However, for regression tests,
// we want to be able to compile these scripts. The regression tests define
// COMPILE_REGRESSION_TEST, and in that case we define FILLMEIN to a non-
// working but innocuous value.
//
#ifdef COMPILE_REGRESSION_TEST
# define FILLMEIN 0
#else
# warning "You must replace the values marked FILLMEIN with real values from the TTN control panel!"
# define FILLMEIN (#dont edit this, edit the lines that use FILLMEIN)
#endif

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t PROGMEM APPEUI[8]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// This should also be in little endian format, see above.
static const u1_t PROGMEM DEVEUI[8]={0xFD, 0x5F, 0x06, 0xD0, 0x7E, 0xD5, 0xB3, 0x70}; //project
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
static const u1_t PROGMEM APPKEY[16]={0x0B, 0xF2, 0xF3, 0xB5, 0x3B, 0x16, 0x2A, 0x11, 0x85, 0x1B, 0x49, 0xAB, 0x26, 0xC1, 0x22, 0x7B}; //project
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

#define MAX_DATA_SIZE 500 // Adjust this value based on your maximum expected payload size

uint8_t mydata[MAX_DATA_SIZE];

static osjob_t sendjob;

//added
String receivedMessage="";
SoftwareSerial mySerial(8,9);

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

// Pin mapping
const lmic_pinmap lmic_pins = {
  .nss = 10,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 7,
  .dio = {2, 5, 6},
};

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              Serial.print("netid: ");
              Serial.println(netid, DEC);
              Serial.print("devaddr: ");
              Serial.println(devaddr, HEX);
              Serial.print("AppSKey: ");
              for (size_t i=0; i<sizeof(artKey); ++i) {
                if (i != 0)
                  Serial.print("-");
                printHex2(artKey[i]);
              }
              Serial.println("");
              Serial.print("NwkSKey: ");
              for (size_t i=0; i<sizeof(nwkKey); ++i) {
                      if (i != 0)
                              Serial.print("-");
                      printHex2(nwkKey[i]);
              }
              Serial.println();
            }
            // Disable link check validation (automatically enabled
            // during join, but because slow data rates change max TX
	    // size, we don't use it in this example.
            LMIC_setLinkCheckMode(0);
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_RFU1:
        ||     Serial.println(F("EV_RFU1"));
        ||     break;
        */
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.print(F("Received "));
              Serial.print(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_SCAN_FOUND:
        ||    Serial.println(F("EV_SCAN_FOUND"));
        ||    break;
        */
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        case EV_TXCANCELED:
            Serial.println(F("EV_TXCANCELED"));
            break;
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            break;

        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}

// original
// void do_send(osjob_t* j){
//     // Check if there is not a current TX/RX job running
//     if (LMIC.opmode & OP_TXRXPEND) {
//         Serial.println(F("OP_TXRXPEND, not sending"));
//     } else {

//         // static uint8_t mydata[] = receivedMessage;
//         // Prepare upstream data transmission at the next possible time.
//         LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
//         Serial.println(F("Packet queued"));
//     }
//     // Next TX is scheduled after TX_COMPLETE event.
// }



//string
// void do_send(osjob_t* j){
//     Serial.println("do_send1");
//     // Check if there is not a current TX/RX job running
//     if (LMIC.opmode & OP_TXRXPEND) {
//         Serial.println(F("OP_TXRXPEND, not sending"));
//     } else {  
//         Serial.println("asdasd");

//         if (mySerial.available() > 0) {
//           Serial.println("Ran here");
//           char receivedChar = mySerial.read();
//           if (receivedChar == '\0') {
//             Serial.println("empty");
//           } else {
//             Serial.println("Ran here 2");
//             receivedMessage += receivedChar;
//             uint8_t mydata[36]; // Create a byte array to hold the converted string

//             // Convert the string to a byte array
//             for (int i = 0; i < 36; i++) {
//               mydata[i] = receivedMessage[i];
//             }
//             if (receivedChar == '\n') {
//               LMIC_setTxData2(1, mydata, 35, 0); 
//               Serial.println(F("Packet queued"));
//               }

//         receivedMessage = ""; // Reset the received message
//         Serial.print("asdasd23");
//     }
//     // Next TX is scheduled after TX_COMPLETE event.
//     Serial.print("asdasd25");
//   }

//   Serial.println("asdasd27");
// }
// Serial.println("do_send22");

// }

//testing
// void do_send(osjob_t* j){
//     // Check if there is not a current TX/RX job running
//     if (LMIC.opmode & OP_TXRXPEND) {
//         Serial.println(F("OP_TXRXPEND, not sending"));
//     } else {
//         // Generate random temperature and humidity values
//         uint8_t temperature = random(0, 100); // Random temperature between 0 and 100
//         uint8_t humidity = random(0, 100);    // Random humidity between 0 and 100

//         // Create a byte array to hold the temperature and humidity values
//         uint8_t payload[2];
//         payload[0] = temperature;
//         payload[1] = humidity;

//         // Prepare upstream data transmission at the next possible time.
//         LMIC_setTxData2(1, payload, sizeof(payload), 0);
//         Serial.println(F("Packet queued"));
//     }
//     // Next TX is scheduled after TX_COMPLETE event.
// }

// integrating with AK code
// void do_send(osjob_t* j){
//     if (LMIC.opmode & OP_TXRXPEND) {
//         Serial.println(F("OP_TXRXPEND, not sending"));
//     } else  {
//         char receivedChar = mySerial.read();
//         // Generate random temperature and humidity values
//         receivedMessage += receivedChar;
//         uint8_t temperature = atoi (receivedMessage.c_str ());

//         // Prepare upstream data transmission at the next possible time.
//         LMIC_setTxData2(1, temperature, sizeof(temperature)-1, 0);
//         Serial.println(F("Packet queued"));

//         // Process received message here if needed
//         receivedMessage = ""; // Reset the received message
//     }
// }

//ak test
// void do_send(osjob_t* j){
//     // Check for incoming messages on SoftwareSerial
//     if (mySerial.available() > 0) {
//         char receivedChar = mySerial.read();
//         if (receivedChar == '\0') {
//             Serial.println("empty");
//         } else {
//             receivedMessage += receivedChar;
//             if (receivedChar == '\n') {
//                 Serial.println(receivedMessage);
//                 // Process received message here if needed
//                 receivedMessage = ""; // Reset the received message
//             }
//         }
//     }

//     // Check if there is not a current TX/RX job running
//     if (LMIC.opmode & OP_TXRXPEND) {
//         Serial.println(F("OP_TXRXPEND, not sending"));
//     } else {
//         // Prepare upstream data transmission at the next possible time.
//         LMIC_setTxData2(1, NULL, 0, 0); // Send empty payload
//         Serial.println(F("Packet queued"));
//     }
//     // Next TX is scheduled after TX_COMPLETE event.
// }


void setup() {
    Serial.begin(9600);
    Serial.println("Starting");
    mySerial.begin(9600);
    Serial.println("Starting 1");

    // #ifdef VCC_ENABLE
    // Serial.println("Starting 2");
    // // For Pinoccio Scout boards
    // pinMode(VCC_ENABLE, OUTPUT);
    // digitalWrite(VCC_ENABLE, HIGH);
    // delay(1000);
    // #endif

    Serial.println("Starting 3");
    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
    Serial.println("Starting 4");
    // Start job (sending automatically starts OTAA too)
    // do_send(&sendjob);
    Serial.println("Starting 5");
}

// void loop() {
//     if (mySerial.available() > 0) {
//         char receivedChar = mySerial.read();
//         if (receivedChar != '\0') {
//             receivedMessage += receivedChar;
//             if (receivedChar == '\n') {
//                 Serial.println(receivedMessage);

//                 // Convert receivedMessage to a uint8_t array
//                 uint8_t mydata[receivedMessage.length() + 1]; // Add 1 for null terminator
//                 strncpy((char *)mydata, receivedMessage.c_str(), receivedMessage.length()); // Copy the message
//                 mydata[receivedMessage.length()] = '\0'; // Null-terminate the array

//                 // Pass mydata to do_send function
//                 do_send(&sendjob);

//                 // Reset receivedMessage
//                 receivedMessage = "";
//             }
//         }
//     }
//     os_runloop_once();
// }

////working stuff here
// void loop() {
//     if (mySerial.available() > 0) {
//         char receivedChar = mySerial.read();
//         if (receivedChar != '\0') {
//             receivedMessage += receivedChar;
//             if (receivedChar == '\n') {
//                 Serial.println(receivedMessage);

//                 // Convert receivedMessage to a uint8_t array
//                 uint8_t mydata[receivedMessage.length() + 1]; // Add 1 for null terminator
//                 for (size_t i = 0; i < receivedMessage.length(); i++) {
//                     mydata[i] = receivedMessage[i]; // Copy character by character
//                 }
//                 mydata[receivedMessage.length()] = '\0'; // Null-terminate the array

//                 // Print the content of mydata
//                 Serial.print("loop Data to send: ");
//                 for (size_t i = 0; i <= receivedMessage.length(); i++) {
//                     Serial.print(mydata[i]);
//                     Serial.print(" ");
//                 }
//                 Serial.println();

//                 // Pass mydata to do_send function
//                 do_send(&sendjob, mydata);

//                 // Reset receivedMessage
//                 receivedMessage = "";
//             }
//         }
//     }
//     os_runloop_once();
// }

// void do_send(osjob_t* j, uint8_t* data) {
//     // Check if there is not a current TX/RX job running
//     if (LMIC.opmode & OP_TXRXPEND) {
//         Serial.println(F("OP_TXRXPEND, not sending"));
//     } else {
//         // Print the contents of data
//         Serial.print(F("dosend Data to send: "));
//         for (size_t i = 0; i < strlen((char*)data); ++i) {
//             Serial.print(data[i], HEX); // Print each byte as hexadecimal
//             Serial.print(" "); // Add a space for readability
//         }
//         Serial.println(); // Add a newline after printing the array
        
//         // Prepare upstream data transmission at the next possible time.
//         LMIC_setTxData2(1, data, strlen((char*)data), 0); // Send data
//         Serial.println(F("Packet queued"));
//     }
//     // Next TX is scheduled after TX_COMPLETE event.
// }
////working stuff end here

//////working even better
// void loop() {
//     static uint8_t port = 1; // Initialize port number
//     static String accumulatedData = ""; // Store accumulated data
    
//     // Read all available data from mySerial and accumulate it
//     while (mySerial.available() > 0) {
//         char receivedChar = mySerial.read();
//         if (receivedChar != '\0') {
//             accumulatedData += receivedChar;
//         }
//     }

//     // Check if a complete message (ending with '\n') is received
//     if (accumulatedData.endsWith("\n")) {
//         Serial.println(accumulatedData);

//         // Convert accumulatedData to a uint8_t array
//         uint8_t mydata[accumulatedData.length() + 1]; // Add 1 for null terminator
//         for (size_t i = 0; i < accumulatedData.length(); i++) {
//             mydata[i] = accumulatedData[i]; // Copy character by character
//         }
//         mydata[accumulatedData.length()] = '\0'; // Null-terminate the array

//         // Pass mydata to do_send function with the current port number
//         do_send(&sendjob, mydata, port);

//         // Toggle between port 1 and port 2
//         port = (port == 1) ? 2 : 1;

//         // Reset accumulatedData
//         accumulatedData = "";
//     }

//     os_runloop_once();
// }

// void do_send(osjob_t* j, uint8_t* data, uint8_t port) {
//     // Check if there is not a current TX/RX job running
//     if (LMIC.opmode & OP_TXRXPEND) {
//         Serial.println(F("OP_TXRXPEND, not sending"));
//     } else {
//         // Print the contents of data
//         Serial.print(F("dosend Data to send: "));
//         for (size_t i = 0; i < strlen((char*)data); ++i) {
//             Serial.print(data[i], HEX); // Print each byte as hexadecimal
//             Serial.print(" "); // Add a space for readability
//         }
//         Serial.println(); // Add a newline after printing the array
        
//         // Prepare upstream data transmission at the next possible time.
//         LMIC_setTxData2(port, data, strlen((char*)data), 0); // Send data with specified port
//         Serial.println(F("Packet queued"));
//     }
//     // Next TX is scheduled after TX_COMPLETE event.
// }
//////working even better end here



void loop() {
    static String accumulatedData = ""; // Store accumulated data
    static unsigned long lastTransmissionTime = 0; // Store the time of the last transmission
    
    // Read all available data from mySerial and accumulate it
    while (mySerial.available() > 0) {
        char receivedChar = mySerial.read();
        if (receivedChar != '\0') {
            accumulatedData += receivedChar;
        }
    }

    // Check if a complete message (ending with '\n') is received
    if (accumulatedData.endsWith("\n")) {
        Serial.println(accumulatedData);

        // Convert accumulatedData to a uint8_t array
        uint8_t mydata[accumulatedData.length() + 1]; // Add 1 for null terminator
        for (size_t i = 0; i < accumulatedData.length(); i++) {
            mydata[i] = accumulatedData[i]; // Copy character by character
        }
        mydata[accumulatedData.length()] = '\0'; // Null-terminate the array

        // Check if 5 seconds have passed since the last transmission
        if (millis() - lastTransmissionTime >= 5000) {
            // Pass mydata to do_send function
            do_send(&sendjob, mydata);

            // Update the time of the last transmission
            lastTransmissionTime = millis();

            // Reset accumulatedData
            accumulatedData = "";
        } else {
            // If less than 5 seconds have passed, delay to maintain the 5-second interval
            delay(5000 - (millis() - lastTransmissionTime));
        }
    }

    os_runloop_once();
}

void do_send(osjob_t* j, uint8_t* data) {
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Print the contents of data
        Serial.print(F("dosend Data to send: "));
        for (size_t i = 0; i < strlen((char*)data); ++i) {
            Serial.print(data[i], HEX); // Print each byte as hexadecimal
            Serial.print(" "); // Add a space for readability
        }
        Serial.println(); // Add a newline after printing the array
        
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, data, strlen((char*)data), 0); // Send data with default port 1
        Serial.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}


// void loop() {
//   if (mySerial.available() > 0) { 
//     char receivedChar = mySerial.read(); 
//     if (receivedChar == '\0') { 
//       Serial.println("empty"); 
//     } else { 
//       receivedMessage += receivedChar; 
       
//       // if (receivedChar == '\n') { 
 
//       //   Serial.println(receivedMessage); 
//       //   char test[] = receivedMessage;
//       //   size_t length = strlen(test);
//       //   Serial.println(length);
//       //   // Serial.println(strlen(receivedMessage));
//       //   // Process received message here if needed 
        
//       //   receivedMessage = ""; // Reset the received message 
//       // } 
//       if (receivedChar == '\n') { 
//         Serial.println(receivedMessage); 

//         // Create a new array to hold the copied string
//         char test[receivedMessage.length() + 1]; // Add 1 for null terminator
        
//         // Copy the contents of receivedMessage to test
//         strcpy(test, receivedMessage.c_str()); // Use c_str() to get the const char* for strcpy

//         // Calculate the length of the copied string
//         size_t length = strlen(test);
//         Serial.println(length);
        
//         // Process received message here if needed 
//         receivedMessage = ""; // Reset the received message 
//       } 
//     } 
//   }
//   os_runloop_once();
    
// }
