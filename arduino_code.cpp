#include <rilso-project-1_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include <esp_camera.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

/* Camera model (AI Thinker) */
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

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE       3

static bool is_initialised = false;
uint8_t *snapshot_buf = nullptr;
static bool debug_nn = false;

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

/* BLE utility function */
void ble_printf(const char *msg) {
  Serial.print(msg);
  if (deviceConnected && pCharacteristic != nullptr) {
    pCharacteristic->setValue((uint8_t*)msg, strlen(msg));
    pCharacteristic->notify();
    delay(10); // Allow buffer time
  }
}

bool ei_camera_init() {
  if (is_initialised) return true;
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, 0);
  }
  is_initialised = true;
  return true;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
  if (!is_initialised) return false;
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;
  bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);
  esp_camera_fb_return(fb);
  if (!converted) return false;

  if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS)
      || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
    ei::image::processing::crop_and_interpolate_rgb888(
      out_buf,
      EI_CAMERA_RAW_FRAME_BUFFER_COLS,
      EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
      out_buf,
      img_width,
      img_height);
  }

  return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
  size_t pixel_ix = offset * 3;
  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = (snapshot_buf[pixel_ix + 2] << 16) |
                 (snapshot_buf[pixel_ix + 1] << 8) |
                 (snapshot_buf[pixel_ix]);
    pixel_ix += 3;
  }
  return 0;
}

/* BLE Callbacks */
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32-CAM-INFER");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  pServer->getAdvertising()->start();

  ble_printf("BLE Inference device ready.\n");

  if (!ei_camera_init()) {
    ble_printf("Failed to initialize camera.\n");
  } else {
    ble_printf("Camera initialized.\n");
  }
}

void loop() {
  if (!deviceConnected) {
    delay(500);
    return;
  }

  snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS *
                                   EI_CAMERA_RAW_FRAME_BUFFER_ROWS *
                                   EI_CAMERA_FRAME_BYTE_SIZE);
  if (!snapshot_buf) {
    ble_printf("Failed to allocate buffer.\n");
    return;
  }

  if (!ei_camera_capture(EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf)) {
    ble_printf("Failed to capture image.\n");
    free(snapshot_buf);
    return;
  }

  ei::signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal.get_data = &ei_camera_get_data;

  ei_impulse_result_t result = { 0 };
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);

  if (err != EI_IMPULSE_OK) {
    ble_printf("Inference failed.\n");
    free(snapshot_buf);
    return;
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "Timing (DSP: %d ms, Class: %d ms, Anomaly: %d ms)\n",
           result.timing.dsp, result.timing.classification, result.timing.anomaly);
  ble_printf(buf);

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
  float threshold = 0.7;
  for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
    auto bb = result.bounding_boxes[i];
    if (bb.value < threshold) continue;
    snprintf(buf, sizeof(buf), "Detected %s (%f) at [%u,%u,%u,%u]\n",
             bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
    ble_printf(buf);
  }
#else
  for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    snprintf(buf, sizeof(buf), "%s: %.5f\n",
             ei_classifier_inferencing_categories[i],
             result.classification[i].value);
    ble_printf(buf);
  }
#endif

#if EI_CLASSIFIER_HAS_ANOMALY == 1
  snprintf(buf, sizeof(buf), "Anomaly: %.3f\n", result.anomaly);
  ble_printf(buf);
#endif

  free(snapshot_buf);
  delay(2000);  // Frame interval
}