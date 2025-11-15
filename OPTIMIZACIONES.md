# Optimizaciones de Memoria para ESP32-S3-EYE

## Problema Original
Error: `E (1246) cam_hal: cam_dma_config(300): frame buffer malloc failed`

**Causa:** Memoria insuficiente para alojar los buffers de la cámara (~230KB).

## Optimizaciones Implementadas

### 1. Reducción de Frame Buffers (camera.cpp:32)
- **Antes:** `fb_count = 2` → ~230KB en PSRAM
- **Ahora:** `fb_count = 1` → ~115KB en PSRAM
- **Ahorro:** ~115KB

### 2. Eliminación de Copia Innecesaria (camera_ui.cpp:35)
- **Antes:** `malloc(fb->len)` + `memcpy()` → 115KB adicional en cada frame
- **Ahora:** `ws_draw_set_frame_direct()` → Usa directamente el buffer de la cámara
- **Ahorro:** ~115KB por frame

### 3. Reducción de Buffers JSON (websocket_client.cpp:144)
- **Antes:** `DynamicJsonDocument(4096)`
- **Ahora:** `DynamicJsonDocument(2048)`
- **Ahorro:** 2KB

### 4. Optimización de Colas FreeRTOS (camara.ino:66-67)
- **Antes:** `captureQueue(2)`, `detectionQueue(4)`
- **Ahora:** `captureQueue(1)`, `detectionQueue(2)`
- **Ahorro:** ~24 bytes

### 5. Orden de Inicialización Optimizado (camara.ino:30)
- **Antes:** WiFi → Display → Cámara
- **Ahora:** Cámara → Display → WiFi
- **Beneficio:** Máxima memoria disponible cuando se inicializa la cámara

### 6. Diagnóstico de Memoria
- Añadido monitoreo de Heap y PSRAM en tiempo de ejecución
- Permite identificar problemas de memoria fácilmente

## Ahorro Total de RAM: ~232KB

## Configuración Requerida para PSRAM

### Arduino IDE:
1. Tools → Board → ESP32S3 Dev Module
2. Tools → PSRAM → "OPI PSRAM"
3. Tools → Flash Mode → "QIO 80MHz"
4. Tools → Partition Scheme → "8M with spiffs"

### PlatformIO (si lo usas):
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi
board_build.partitions = default_8MB.csv
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
```

## Mejoras Adicionales

### Rendimiento:
- **XCLK aumentado:** 10MHz → 20MHz (mejor framerate)
- **Eliminación de copias:** Dibujo directo desde buffer de cámara

### Estabilidad:
- Menos fragmentación de memoria
- Menor riesgo de fallos por OOM (Out Of Memory)

### Eficiencia de CPU:
- Menos operaciones de `memcpy()`
- Menos tiempo en secciones críticas

## Verificación

Después de cargar el código, verifica en el Monitor Serial:
```
=== DIAGNÓSTICO DE MEMORIA ===
Heap libre: XXXXX bytes
PSRAM libre: XXXXX bytes  <-- Debe ser > 0
PSRAM total: XXXXX bytes  <-- Debe ser ~2MB o más

📷 Inicializando cámara...
✅ Cámara inicializada
```

Si `PSRAM total: 0 bytes`, **PSRAM no está habilitada** → Revisa la configuración del IDE.

## Próximos Pasos Opcionales

1. **Reducir resolución** (si 240x240 sigue dando problemas):
   - Cambiar a `FRAMESIZE_QVGA` (320x240) o `FRAMESIZE_QQVGA` (160x120)

2. **Ajustar FPS** (camera_ui.cpp:29):
   - Aumentar `frameDelay` para reducir carga de CPU

3. **Optimizar WebSocket**:
   - Comprimir frames antes de enviar
   - Reducir calidad/resolución para transmisión

4. **Usar JPEG** (en lugar de RGB565):
   - Menor tamaño de buffer en tránsito
   - Mayor compresión para WebSocket
   - Requiere conversión en display
