#include <esp_camera.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// CAMERA PINS for AI Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

camera_config_t camera_config = {
  .pin_pwdn       = PWDN_GPIO_NUM,
  .pin_reset      = RESET_GPIO_NUM,
  .pin_xclk       = XCLK_GPIO_NUM,
  .pin_sscb_sda   = SIOD_GPIO_NUM,
  .pin_sscb_scl   = SIOC_GPIO_NUM,
  .pin_d7         = Y9_GPIO_NUM,
  .pin_d6         = Y8_GPIO_NUM,
  .pin_d5         = Y7_GPIO_NUM,
  .pin_d4         = Y6_GPIO_NUM,
  .pin_d3         = Y5_GPIO_NUM,
  .pin_d2         = Y4_GPIO_NUM,
  .pin_d1         = Y3_GPIO_NUM,
  .pin_d0         = Y2_GPIO_NUM,
  .pin_vsync      = VSYNC_GPIO_NUM,
  .pin_href       = HREF_GPIO_NUM,
  .pin_pclk       = PCLK_GPIO_NUM,
  .xclk_freq_hz   = 20000000,
  .ledc_timer     = LEDC_TIMER_0,
  .ledc_channel   = LEDC_CHANNEL_0,
  .pixel_format   = PIXFORMAT_JPEG,
  .frame_size     = FRAMESIZE_QVGA,
  .jpeg_quality   = 12,
  .fb_count       = 1,
  .fb_location    = CAMERA_FB_IN_PSRAM,
  .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
};

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(1000);
  Serial.println("Booting...");

  // Initialize camera
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  Serial.println("Camera init success");

  // Init BLE
  BLEDevice::init("ESP32-CAM");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("Waiting for BLE client to connect...");
}

void loop() {
  if (deviceConnected) {
    Serial.println("Capturing frame...");
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    Serial.printf("Captured %d bytes\n", fb->len);

    // Send JPEG in 512-byte chunks
    size_t sent = 0;
    const size_t chunk_size = 512;

    while (sent < fb->len) {
      size_t remain = fb->len - sent;
      size_t len = (remain < chunk_size) ? remain : chunk_size;
      pCharacteristic->setValue(fb->buf + sent, len);
      pCharacteristic->notify();
      sent += len;
      delay(10); // allow BLE buffer to catch up
    }

    esp_camera_fb_return(fb);
    Serial.println("Image sent.\n");

    delay(2000);  // wait before next frame
  }
}
