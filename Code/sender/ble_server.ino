#include <NimBLEDevice.h>

NimBLECharacteristic* pCharacteristic;
bool deviceConnected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        deviceConnected = true;
        Serial.println("Client verbunden");
    }
    void onDisconnect(NimBLEServer* pServer) override {
        deviceConnected = false;
        Serial.println("Client getrennt");
        pServer->startAdvertising();
    }
};

void setup() {
    Serial.begin(115200);

    // BLE Initialisierung
    NimBLEDevice::init("NotifyServer");

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Service erstellen
    NimBLEService* pService = pServer->createService("1234");

    // Characteristic erstellen – READ + NOTIFY
    pCharacteristic = pService->createCharacteristic(
        "ABCD",
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    pCharacteristic->setValue("Initial");

    pService->start();

    // Werbung starten
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID("1234");
    pAdvertising->start();

    Serial.println("BLE Server gestartet, warte auf Client...");
}

void loop() {
    static uint32_t counter = 0;

    if (deviceConnected) {
        counter++;

        // neuen Wert setzen
        String value = "Wert: " + String(counter);
        pCharacteristic->setValue(value.c_str());

        // Notify senden
        pCharacteristic->notify();

        Serial.print("Gesendet: ");
        Serial.println(value);

        delay(1000);  // jede Sekunde
    }

    delay(10);
}
