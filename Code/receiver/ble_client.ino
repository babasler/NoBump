#include <NimBLEDevice.h>

static NimBLEAdvertisedDevice* advDevice;
static NimBLEClient* client;

class NotifyCallback : public NimBLERemoteCharacteristicCallbacks {
    void onNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) override {
        Serial.print("Notification empfangen: ");
        Serial.println(std::string((char*)data, len).c_str());
    }
};

class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        if (advertisedDevice->isAdvertisingService(NimBLEUUID("1234"))) {
            Serial.println("Gesuchter Service gefunden!");
            advDevice = advertisedDevice;
            NimBLEDevice::getScan()->stop();
        }
    }
};

void connectToServer() {
    client = NimBLEDevice::createClient();
    Serial.println("Verbinde...");

    client->connect(advDevice);

    Serial.println("Verbunden!");

    NimBLERemoteService* service = client->getService("1234");
    NimBLERemoteCharacteristic* chr = service->getCharacteristic("ABCD");

    // Subscribe aktivieren
    chr->subscribe(true, new NotifyCallback());

    Serial.println("Subscribed für Notifications!");
}

void setup() {
    Serial.begin(115200);
    NimBLEDevice::init("");

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(45);
    scan->setWindow(15);
    scan->setCallbacks(new ScanCallbacks(), true);
    scan->start(5, false);

    Serial.println("Suche nach BLE Geräten...");
}

void loop() {
    if (advDevice && !client) {
        connectToServer();
    }

    delay(100);
}
