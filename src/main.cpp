// Include Libraries
#include <WiFi.h>
#include <esp_now.h>
#include <mbedtls/aes.h>
#include <set>
#include <queue>
#include <main.h>

using namespace std;

// Define Message Prefix
const String message_header = MESSAGE_HEADER;
const String gpio_prefix = GPIO_PREFIX;

// Enable Auto Relay Reset
unsigned long relayOnTime = 0;
const unsigned long relayDuration = RELAY_DURATION;

// Enable Local Relay
bool localRelay = LOCAL_RELAY;

// Encryption Key
const char* key = AES_KEY; // 16 bytes key for AES-128
// byte iv[16] = {0xce, 0x45, 0x81, 0xcd, 0xd2, 0x20, 0x27, 0xd0, 0x66, 0x79, 0x34, 0x96, 0xc2, 0x7d, 0xe6, 0x65}; // Static IV

// Initialize  Relay and pushbutton state
bool buttonDown = false;
bool relayOn = false;

// Set and Queue to store RandomID values
set<String> randomIDSet;
queue<String> randomIDQueue;


byte encrypted[128];
char decrypted[128];


int encryptAES(const char* plaintext, const char* key, byte* encrypted, byte* iv) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, (const unsigned char*)key, 128);

  int plaintextLength = strlen(plaintext);
  int length = ((plaintextLength + 15) / 16) * 16; // Padded to 16 bytes

  // Copy plaintext into a buffer and pad it
  byte buffer[length];
  memset(buffer, 0, length);
  memcpy(buffer, plaintext, plaintextLength);

  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, length, iv, buffer, encrypted);

  mbedtls_aes_free(&aes);
  return length;
}


int decryptAES(const byte* encrypted, int length, const char* key, char* decrypted, byte* iv) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, (const unsigned char*)key, 128);

  byte buffer[length];
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iv, encrypted, buffer);

  // Copy and null-terminate the decrypted string
  memcpy(decrypted, buffer, length);
  decrypted[length] = '\0';

  mbedtls_aes_free(&aes);
  return length;
}


int getRandomID() {
  return random(1000, 2000000000);
}


void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength) {
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}


bool isNewRandomID(const String &randomID) {
  if (randomIDSet.find(randomID) == randomIDSet.end()) {
    // RandomID not found, so it's new
    randomIDSet.insert(randomID);
    randomIDQueue.push(randomID);
    
    // If the queue exceeds MAX_RANDOM_ID, remove the oldest
    if (randomIDQueue.size() > MAX_RANDOM_ID) {
      String oldRandomID = randomIDQueue.front();
      randomIDQueue.pop();
      randomIDSet.erase(oldRandomID);
    }
    
    return true;
  }
  // RandomID already exists
  return false;
}


void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen) {
  
  // Format the MAC address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);

  // Check if the received message starts with the message header
  if (dataLen < message_header.length() || strncmp((char*)data, message_header.c_str(), message_header.length()) != 0) {
    Serial.printf("%s: Message does not start with the expected header.", macStr);
    Serial.println();
    return;
  }

  // Only allow a maximum of 250 characters in the message + a null terminating byte
  byte encryptedBuffer[ESP_NOW_MAX_DATA_LEN];
  int encryptedLength = dataLen - message_header.length();
  memcpy(encryptedBuffer, data + message_header.length(), encryptedLength);
  
  // Print the encrypted data
  Serial.println("Received Encrypted message:");
  for (int i = 0; i < encryptedLength; i++) {
    Serial.printf("%02X ", encryptedBuffer[i]);
  }
  Serial.println();

  decryptAES(encryptedBuffer, encryptedLength, key, decrypted, iv);

  // Make sure we are null terminated
  decrypted[encryptedLength] = 0;

  // Send Debug log message to the serial port
  Serial.printf("Received message from: %s - %s\n", macStr, decrypted);

  // Extract Random ID and Store
  String message(decrypted);
  String randomID;
  String sCommand;
  int delimiter;

  delimiter = message.indexOf(";");
  sCommand = message.substring(0, delimiter);
  randomID = message.substring(delimiter + 1, message.length());

  // Check mac address
  // if (strcmp("30:ae:a4:dd:9e:54", macStr) == 1) {
  //   Serial.println("Mac Address Not Matched");
  //   return;
  // } else {
  //   Serial.println("Mac Address Matched");
  // }

  // Check if RandomID is new
  if (!isNewRandomID(randomID)) {
    Serial.println("Random ID already used");
    return;
  }

  // Check switch status

  String switch_command = gpio_prefix;
  switch_command += STATUS_BUTTON;
  switch_command += "+ON";

  if (sCommand.compareTo(switch_command) == 0) {
    relayOn = true;
    relayOnTime = millis(); // Record the time when the relay was turned on
  } else {
    relayOn = false;
  }
  digitalWrite(STATUS_RELAY, relayOn);
  delay(500);
}

void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status) {
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void broadcast(const String &message) {
  // Encrypt the message
  int encryptedLength = encryptAES(message.c_str(), key, encrypted, iv);

  // Create a buffer to hold the final message
  int headerLength = message_header.length();
  int totalLength = headerLength + encryptedLength;
  uint8_t finalMessage[totalLength];

    // Copy the header to the final message
  memcpy(finalMessage, message_header.c_str(), headerLength);

  // Copy the encrypted message after the header
  memcpy(finalMessage + headerLength, encrypted, encryptedLength);

  // Broadcast a message to every device in range
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_add_peer(&peerInfo);
  }

  // Print the encrypted data
  Serial.println("Encrypted message with header:");
  for (int i = 0; i < totalLength; i++) {
    Serial.printf("%02X ", finalMessage[i]);
  }
  Serial.println();

  // Send message
  esp_err_t result = esp_now_send(broadcastAddress, finalMessage, totalLength);

  // Print results to serial monitor
  if (result == ESP_OK) {
    Serial.println("Broadcast message success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    Serial.println("ESP-NOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Unknown error");
  }

}

void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);
  delay(1000);

  // Set ESP32 in STA mode to begin with
  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Broadcast Remote Control");

  // Print MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Disconnect from WiFi
  WiFi.disconnect();

  // Initialize ESP-NOW
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESP-NOW Init Success");
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
  } else {
    Serial.println("ESP-NOW Init Failed");
    delay(3000);
    ESP.restart();
  }

  // Pushbutton uses built-in pullup resistor
  pinMode(STATUS_BUTTON, INPUT_PULLUP);

  // Relay Output
  pinMode(STATUS_RELAY, OUTPUT);
}

void loop() {
  String message = gpio_prefix;
  message += STATUS_BUTTON;

  // Reset Relay if relayDuration is defined.
  
  if (relayOn && (relayDuration > 0) && (millis() - relayOnTime >= relayDuration)) {
    relayOn = false;
    digitalWrite(STATUS_RELAY, relayOn);
    Serial.println("Relay turned off after 5 seconds.");
  }


  if (digitalRead(STATUS_BUTTON)) {
    // Detect the transition from low to high
    if (!buttonDown) {
      buttonDown = true;
      if (localRelay)
      {
         digitalWrite(STATUS_RELAY, false);
 
      }
      message += "+OFF;";
      message += getRandomID();
      broadcast(message);
      Serial.println(message);
    }
    delay(500);
  } else {
    // Reset the button state
    if (buttonDown) {
      buttonDown = false;
      if (localRelay)
      {
         digitalWrite(STATUS_RELAY, true);
 
      }
      message += "+ON;";
      message += getRandomID();
      broadcast(message);
      Serial.println(message);
    }
    delay(500);
  }
}
