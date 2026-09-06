#include "ble_manager.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include "battery_manager.h"
#include "device_config.h"
#include "wifi_manager.h"

namespace {
BLECharacteristic *statusCharacteristic = nullptr;

class ProvisioningCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    const std::string raw = characteristic->getValue();
    saveProvisioning(String(raw.c_str()));
  }
};

class PairingServerCallbacks : public BLEServerCallbacks {
  void onDisconnect(BLEServer *server) override {
    server->startAdvertising();
  }
};
}

void initBle() {
  BLEDevice::init((String("Companion-") + DeviceConfig::DEVICE_ID).c_str());
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new PairingServerCallbacks());
  BLEService *service = server->createService(DeviceConfig::BLE_SERVICE_UUID);

  BLECharacteristic *provisioning = service->createCharacteristic(
      DeviceConfig::PROVISIONING_UUID, BLECharacteristic::PROPERTY_WRITE);
  provisioning->setCallbacks(new ProvisioningCallbacks());

  statusCharacteristic = service->createCharacteristic(
      DeviceConfig::STATUS_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  statusCharacteristic->addDescriptor(new BLE2902());
  service->start();
  server->getAdvertising()->addServiceUUID(DeviceConfig::BLE_SERVICE_UUID);
  server->startAdvertising();
}

void publishStatus() {
  if (statusCharacteristic == nullptr) return;
  String status = String("device_id=") + DeviceConfig::DEVICE_ID + ";provisioned=" +
                  (isProvisioned() ? "1" : "0") + ";wifi=" +
                  (isWifiConnected() ? "1" : "0") + ";battery=" +
                  String(getBatteryPercent());
  statusCharacteristic->setValue(status.c_str());
  statusCharacteristic->notify();
}
