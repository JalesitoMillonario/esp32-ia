#include "camera.h"

bool camera_flip_vertical_state = 0;
bool camera_mirror_horizontal_state = 1;

int camera_init(void) {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;  // Aumentar frecuencia para mejor rendimiento
  config.frame_size = FRAMESIZE_240X240;
  config.pixel_format = PIXFORMAT_RGB565;  // RGB565 (ESTABLE)
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;  // Requiere PSRAM habilitado
  config.jpeg_quality = 12;  // No se usa con RGB565
  config.fb_count = 1;  // OPTIMIZADO: Reducir de 2 a 1 buffer (-115KB)

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\r\n", err);
    return 0;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, camera_flip_vertical_state);
  s->set_hmirror(s, camera_mirror_horizontal_state);
  s->set_brightness(s, 0);
  s->set_saturation(s, 0);

  Serial.println("Camera configuration complete!");
  return 1;
}

bool camera_get_flip_vertical(void) { return camera_flip_vertical_state; }
bool camera_get_mirror_horizontal(void) { return camera_mirror_horizontal_state; }

void camera_set_flip_vertical(bool state) {
  sensor_t * s = esp_camera_sensor_get();
  camera_flip_vertical_state = state;
  s->set_vflip(s, camera_flip_vertical_state);
}

void camera_set_mirror_horizontal(bool state) {
  sensor_t * s = esp_camera_sensor_get();
  camera_mirror_horizontal_state = state;
  s->set_hmirror(s, camera_mirror_horizontal_state); 
}
