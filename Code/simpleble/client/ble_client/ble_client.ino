#include <NimBLEDevice.h>

static const NimBLEAdvertisedDevice* advDevice;
static bool                          doConnect  = false;
static bool                          connected  = false;
static uint32_t                      scanTimeMs = 5000; // Scan duration in milliseconds 0 = forever


#define DEBUG
#define UUID_NOBUMP_SERVICE      "AFFE"
#define UUID_DOORSTATE_CHAR      "BEEF"
#define D1 1
#define D2 2
#define D7 17
#define D8 19

enum class DoorState{
  OPEN,
  CLOSED,
  ERROR
};


class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        connected = true;
        Serial.printf("Connected\n");
    }

    void onDisconnect(NimBLEClient* pClient, int reason) override {
        connected = false;
        #ifdef DEBUG
        Serial.printf("%s Disconnected, reason = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
        #endif
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
} clientCallbacks;

class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        #ifdef DEBUG
        Serial.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());
        #endif
        if (advertisedDevice->isAdvertisingService(NimBLEUUID(UUID_NOBUMP_SERVICE))) {
            #ifdef DEBUG
            Serial.printf("Found Our Service\n");
            #endif
            NimBLEDevice::getScan()->stop();
            advDevice = advertisedDevice;
            doConnect = true;
        }
    }
    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        #ifdef DEBUG
        Serial.printf("Scan Ended, reason: %d, device count: %d\n", reason, results.getCount());
        #endif
        if (!connected && !doConnect) {
            #ifdef DEBUG
            Serial.printf("Restarting scan\n");
            #endif
            NimBLEDevice::getScan()->start(scanTimeMs, false, true);
        }
    }
} scanCallbacks;

void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    #ifdef DEBUG
    std::string str  = (isNotify == true) ? "Notification" : "Indication";
    str             += " from ";
    str             += pRemoteCharacteristic->getClient()->getPeerAddress().toString();
    str             += ": Service = " + pRemoteCharacteristic->getRemoteService()->getUUID().toString();
    str             += ", Characteristic = " + pRemoteCharacteristic->getUUID().toString();
    str             += ", Value = " + std::string((char*)pData, length);
    Serial.printf("%s\n", str.c_str());
    #endif
    if (length < 1) {
        #ifdef DEBUG
        Serial.printf("Received data length is less than 1, ignoring\n");
        #endif
        return;
    }
    std::string value = std::string((char*)pData, length);
    #ifdef DEBUG
    Serial.printf("Received State is ");
    Serial.printf(value.c_str());
    #endif
    DoorState receivedState = value == "OPEN" ? DoorState::OPEN : (value == "CLOSED" ? DoorState::CLOSED : DoorState::ERROR);
    toggleLEDsFromDoorState(receivedState);
}
bool connectToServer() {
    NimBLEClient* pClient = nullptr;
    if (NimBLEDevice::getCreatedClientCount()) {
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if (pClient) {
            if (!pClient->connect(advDevice, false)) {
                #ifdef DEBUG
                Serial.printf("Reconnect failed\n");
                #endif
                return false;
            }
            #ifdef DEBUG
            Serial.printf("Reconnected client\n");
            #endif
        } else {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }
    if (!pClient) {
        if (NimBLEDevice::getCreatedClientCount() >= MYNEWT_VAL(BLE_MAX_CONNECTIONS)) {
            #ifdef DEBUG
            Serial.printf("Max clients reached - no more connections available\n");
            #endif
            return false;
        }
        pClient = NimBLEDevice::createClient();
        #ifdef DEBUG
        Serial.printf("New client created\n");
        #endif
        pClient->setClientCallbacks(&clientCallbacks, false);
        pClient->setConnectionParams(12, 12, 0, 150);
        pClient->setConnectTimeout(5 * 1000);
        if (!pClient->connect(advDevice)) {
            NimBLEDevice::deleteClient(pClient);
            #ifdef DEBUG
            Serial.printf("Failed to connect, deleted client\n");
            #endif
            return false;
        }
    }
    if (!pClient->isConnected()) {
        if (!pClient->connect(advDevice)) {
            #ifdef DEBUG
            Serial.printf("Failed to connect\n");
            #endif
            return false;
        }
    }
    #ifdef DEBUG
    Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());
    #endif

    NimBLERemoteService*        pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor*     pDsc = nullptr;

    pSvc = pClient->getService(UUID_NOBUMP_SERVICE);
    if (pSvc) {
        pChr = pSvc->getCharacteristic(UUID_DOORSTATE_CHAR );
    }
        if (pChr) {
        if (pChr->canNotify()) {
            if (!pChr->subscribe(true, notifyCB)) {
                pClient->disconnect();
                #ifdef DEBUG
                Serial.printf("Subscribe failed\n");
                #endif
                return false;
            }
            #ifdef DEBUG
            Serial.printf("Subscribed Characteristic\n");
            #endif
        }
    } else {
        #ifdef DEBUG
        Serial.printf("DEAD service not found.\n");
        #endif
    }
    #ifdef DEBUG
    Serial.printf("Done with this device!\n");
    #endif
    return true;
}

void toggleLEDsFromDoorState(DoorState state)
{
  if (state == DoorState::OPEN)
  {
    #ifdef DEBUG
    Serial.printf("Turning LEDs On");
    #endif
    digitalWrite(D8, HIGH);
    digitalWrite(D1, HIGH);
    digitalWrite(D2, HIGH);
    digitalWrite(D7, HIGH);
  }
  else if (state == DoorState::CLOSED)
  {
    #ifdef DEBUG
    Serial.printf("Turning LEDs Off");
    #endif
    digitalWrite(D8, LOW);
    digitalWrite(D1, LOW);
    digitalWrite(D2, LOW);
    digitalWrite(D7, LOW);
  }
}

void setup() {
    #ifdef DEBUG
    Serial.begin(115200);
    Serial.printf("Starting NimBLE Client\n");
    #endif
    NimBLEDevice::init("NoBump-Client");

    NimBLEDevice::setPower(3);
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(&scanCallbacks, false);
    pScan->setInterval(100);
    pScan->setWindow(100);
    pScan->setActiveScan(true);
    pScan->start(scanTimeMs);
    #ifdef DEBUG
    Serial.printf("Scanning for peripherals\n");
    #endif

    // Pins der LEDs als Output setzen
    pinMode(D8, OUTPUT);
    pinMode(D1, OUTPUT);
    pinMode(D2, OUTPUT);
    pinMode(D7, OUTPUT);

    // Pin für Spannungsmessung als Input setzen
    //pinMode(A0, INPUT);
}

void loop() {
    delay(10);

    if (doConnect) {
        doConnect = false;
        if (connectToServer()) {
            #ifdef DEBUG
            Serial.printf("Success! we should now be getting notifications\n");
            #endif
        } else {
            #ifdef DEBUG
            Serial.printf("Failed to connect, starting scan\n");
            #endif
            NimBLEDevice::getScan()->start(scanTimeMs, false, true);
        }
    }
}