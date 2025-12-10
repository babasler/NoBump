#include <NimBLEDevice.h>
//Er schreibt auf die Characteristic, deswegen ist das der Sender -> Misst den Doorstate

static NimBLEServer* pServer;
#define UUID_NOBUMP_SERVICE      "12345678-0001-0000-0000-000000000001"
#define UUID_DOORSTATE_CHAR      "12345678-0001-0000-0000-000000000002"


class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        #ifdef DEBUG
        Serial.printf("Client address: %s\n", connInfo.getAddress().toString().c_str());
        #endif
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        #ifdef DEBUG
        Serial.printf("Client disconnected - start advertising\n");
        #endif
        NimBLEDevice::startAdvertising();
    }
} serverCallbacks;

void setup(void) {
    #ifdef DEBUG
    Serial.begin(115200);
    Serial.printf("Starting NoBump Server\n");
    #endif

    NimBLEDevice::init("NoBump");

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    NimBLEService*        pDeadService = pServer->createService(UUID_NOBUMP_SERVICE);
    NimBLECharacteristic* pBeefCharacteristic =
        pDeadService->createCharacteristic(UUID_DOORSTATE_CHAR, NIMBLE_PROPERTY::NOTIFY 
                                          
        );
    pDeadService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("NoBump-Server");
    pAdvertising->addServiceUUID(pDeadService->getUUID());
    pAdvertising->enableScanResponse(false);
    pAdvertising->start();
    #ifdef DEBUG
    Serial.printf("Advertising Started\n");
    #endif
}

void loop() {
    delay(2000);

    if (pServer->getConnectedCount()) {
        NimBLEService* pSvc = pServer->getServiceByUUID(UUID_NOBUMP_SERVICE);
        if (pSvc) {
            NimBLECharacteristic* pChr = pSvc->getCharacteristic(UUID_DOORSTATE_CHAR);
            if (pChr) {
                pChr->setValue("Hello");
                pChr->notify();
            }
        }
    }
}