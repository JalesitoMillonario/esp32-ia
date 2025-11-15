#pragma once
#include <Arduino.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

void create_camera_task(QueueHandle_t captureQueue, QueueHandle_t detectionQueue, SemaphoreHandle_t captureMutex);
void stop_camera_task(void);

// deja la declaración de loopTask_camera normal, sin static
void loopTask_camera(void *pvParameters);
