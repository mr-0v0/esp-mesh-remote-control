#ifndef MAIN_H
#define MAIN_H

#include <WiFi.h>
#include <esp_now.h>
#include <mbedtls/aes.h>
#include <set>
#include <queue>

#define MESSAGE_HEADER "EPRC:"
#define GPIO_PREFIX "GPIO"

// Define Relay and pushbutton pins
#define STATUS_RELAY 16
#define STATUS_BUTTON 5

// Define Max Random ID History
#define MAX_RANDOM_ID 1000

// Enable Auto Relay Reset
#define RELAY_DURATION 2000  // 2 seconds; Disable Relay Auto Reset: relayDuration = 0

// Enable Local Relay
#define LOCAL_RELAY true

#define AES_KEY "1234567890123456" // Change This
byte iv[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06}; // Static IV Change This


// Function Prototypes
int encryptAES(const char* plaintext, const char* key, byte* encrypted, byte* iv);
int decryptAES(const byte* encrypted, int length, const char* key, char* decrypted, byte* iv);
int getRandomID();
void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength);
bool isNewRandomID(const String &randomID);
void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen);
void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status);
void broadcast(const String &message);
void setup();
void loop();

#endif // MAIN_H