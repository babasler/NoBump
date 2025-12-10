/**
 *  NimBLE_Server Demo:
 *
 *  Demonstrates many of the available features of the NimBLE server library.
 *  BLE servers perform 2 tasks, they advertise their existence for clients to find them and they provide services which contain information for the connecting client.
 *  Created: on March 22 2020
 *      Author: H2zero
 */

#include <NimBLEDevice.h>

static NimBLEServer* pServer;

uint32_t i = 0;

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.printf("Client address: %s\n", connInfo.getAddress().toString().c_str());

        /**
         *  We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments.
         */
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("Client disconnected - start advertising\n");
        NimBLEDevice::startAdvertising();
    }
} serverCallbacks;

/** Handler class for characteristic actions */
// class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
//     void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
//         Serial.printf("%s : onRead(), value: %s\n",
//                       pCharacteristic->getUUID().toString().c_str(),
//                       pCharacteristic->getValue().c_str());
//     }

//     void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
//         Serial.printf("%s : onWrite(), value: %s\n",
//                       pCharacteristic->getUUID().toString().c_str(),
//                       pCharacteristic->getValue().c_str());
//     }


//     /** Peer subscribed to notifications/indications */
//     // void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
//     //     std::string str  = "Client ID: ";
//     //     str             += connInfo.getConnHandle();
//     //     str             += " Address: ";
//     //     str             += connInfo.getAddress().toString();
//     //     if (subValue == 0) {
//     //         str += " Unsubscribed to ";
//     //     } else if (subValue == 1) {
//     //         str += " Subscribed to notifications for ";
//     //     } else if (subValue == 2) {
//     //         str += " Subscribed to indications for ";
//     //     } else if (subValue == 3) {
//     //         str += " Subscribed to notifications and indications for ";
//     //     }
//     //     str += std::string(pCharacteristic->getUUID());

//     //     Serial.printf("%s\n", str.c_str());
//     // }
// } chrCallbacks;

/** Handler class for descriptor actions */
// class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
//     void onWrite(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override {
//         std::string dscVal = pDescriptor->getValue();
//         Serial.printf("Descriptor written value: %s\n", dscVal.c_str());
//     }

//     void onRead(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override {
//         Serial.printf("%s Descriptor read\n", pDescriptor->getUUID().toString().c_str());
//     }
// } dscCallbacks;

void setup(void) {
    Serial.begin(115200);
    Serial.printf("Starting NimBLE Server\n");

    /** Initialize NimBLE and set the device name */
    NimBLEDevice::init("NimBLE");

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    NimBLEService*        pDeadService = pServer->createService("DEAD");
    NimBLECharacteristic* pBeefCharacteristic =
        pDeadService->createCharacteristic("BEEF", NIMBLE_PROPERTY::NOTIFY 
                                          //NIMBLE_PROPERTY::READ |
                                          //NIMBLE_PROPERTY::WRITE | 
        );

   //pBeefCharacteristic->setValue("Burger");
   //pBeefCharacteristic->setCallbacks(&chrCallbacks);

    /** Start the services when finished creating all Characteristics and Descriptors */
    pDeadService->start();

    /** Create an advertising instance and add the services to the advertised data */
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("NimBLE-Server");
    pAdvertising->addServiceUUID(pDeadService->getUUID());
    /**
     *  If your device is battery powered you may consider setting scan response
     *  to false as it will extend battery life at the expense of less data sent.
     */
    pAdvertising->enableScanResponse(false);
    pAdvertising->start();

    Serial.printf("Advertising Started\n");
}

void loop() {
    /** Loop here and send notifications to connected peers */
    delay(2000);

    if (pServer->getConnectedCount()) {
        NimBLEService* pSvc = pServer->getServiceByUUID("DEAD");
        if (pSvc) {
            NimBLECharacteristic* pChr = pSvc->getCharacteristic("BEEF");
            if (pChr) {
                pChr->setValue("Hello" + std::to_string(i));
                i = i+ 1;
                pChr->notify();
            }
        }
    }
}