# NimBLE Cheat Sheet (Markdown)
## 🔵 Überblick: Bluetooth Low Energy (BLE)
| Begriff                              | Erklärung                                                                            |
| ------------------------------------ | ------------------------------------------------------------------------------------ |
| **GATT** (Generic Attribute Profile) | Definiert, wie Geräte Daten über Attribute austauschen.                              |
| **Service**                          | Eine Sammlung von Characteristic-Daten (z. B. Battery Service).                      |
| **Characteristic**                   | Ein einzelnes Datenobjekt mit Value und optionalen Properties (READ, WRITE, NOTIFY). |
| **Descriptor**                       | Metadaten zu einer Characteristic (z. B. CCCD für Notifications).                    |
| **UUID**                             | Identifiziert Services/Characteristics (16-Bit, 32-Bit, 128-Bit).                    |
| **Peripheral**                       | BLE-Server, der Daten anbietet.                                                      |
| **Central**                          | BLE-Client, der Services liest/schreibt.                                             |
## 🧭 NimBLE: Grundstruktur
### ➤ Initialisierung eines BLE-Servers
```cpp
#include <NimBLEDevice.h>

void setup() {
    NimBLEDevice::init("MyNimBLEDevice");

    NimBLEServer *server = NimBLEDevice::createServer();

    NimBLEService *service = server->createService("1234");

    NimBLECharacteristic *characteristic = service->createCharacteristic(
        "ABCD",
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );

    characteristic->setValue("Hello BLE!");

    service->start();
    server->getAdvertising()->start();
    Serial.println("BLE server started");
}
```
## 📡 NimBLE API – Wichtigste Funktionen
### 🔧 NimBLEDevice
```cpp
NimBLEDevice::init("DeviceName");
NimBLEDevice::createServer();
NimBLEDevice::getAdvertising();
NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
```
### 🖥️ NimBLEServer
```cpp
NimBLEServer *srv = NimBLEDevice::createServer();
srv->createService("UUID");
srv->getAdvertising();
Callbacks (optional)
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server);
    void onDisconnect(NimBLEServer* server);
};
srv->setCallbacks(new ServerCallbacks());
```
### 📦 NimBLEService
```cpp
NimBLEService *service = srv->createService("UUID");
service->createCharacteristic("UUID", properties);
service->start();
```
### 🧬 NimBLECharacteristic
#### Eigenschaften (Properties)
```cpp
NIMBLE_PROPERTY::READ  
NIMBLE_PROPERTY::WRITE  
NIMBLE_PROPERTY::NOTIFY  
NIMBLE_PROPERTY::INDICAT
```
#### Methoden
```cpp
characteristic->setValue("text");
characteristic->notify();
characteristic->getValue();
characteristic->setCallbacks(callbacks);
```

### 🧩 NimBLEDescriptor

```cpp
NimBLEDescriptor *desc = characteristic->createDescriptor(
    "2901",  // User Description Descriptor
    NIMBLE_PROPERTY::READ,
    20
);
desc->setValue("MyCharacteristic");
```
### 🟢 BLE Advertising (Werbung)
```cpp
NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
adv->addServiceUUID("1234");
adv->setScanResponse(true);
adv->start();
```
## BLE SERVER (ESP32) – sendet alle 1s einen neuen Wert per Notify
```cpp
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
```

## BLE CLIENT (ESP32) – verbindet sich und empfängt Notifications
```cpp
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
```
