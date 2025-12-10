#include <NimBLEDevice.h>

static const NimBLEAdvertisedDevice* advDevice;
static bool                          doConnect  = false;
static bool                          connected  = false;
static uint32_t                      scanTimeMs = 5000; // Scan duration in milliseconds 0 = forever

#define UUID_NOBUMP_SERVICE      "12345678-0001-0000-0000-000000000001"
#define UUID_DOORSTATE_CHAR      "12345678-0001-0000-0000-000000000002"


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
        pChr = pSvc->getCharacteristic(UUID_NOBUMP_CHARACTERISTIC);
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