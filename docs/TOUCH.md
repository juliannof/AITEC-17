# TOUCH — GT911 Capacitivo (iMakie P4)

> ℹ️ **El chip GT911 es el mismo en ambas placas.** Las dimensiones y pines varían:
> - **JC4880P433C (legacy):** 480×800, INT=GPIO9, RST=GPIO10
> - **JC1060P470C (activa):** 1024×600, INT=-1 (NC), RST=-1 (NC), SDA=GPIO7, SCL=GPIO8
> Ver `MASTER_S3-P4/P4_JC1060P470C/src/config.h` para pines actuales.

Documentación del subsistema de touch P4. Panel capacitivo GT911 para navegación LVGL.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (GT911, I2C_NUM_1)

---

## 1. HARDWARE TOUCH

### 1.1 Especificación

| Parámetro | Valor |
|-----------|-------|
| **Chip** | GT911 |
| **Tipo** | Capacitivo (multi-touch) |
| **Interface** | I2C |
| **Resolución** | 480×800 |
| **Presión** | Detecta presión (força) |

### 1.2 Pinout P4

```
GT911 (I2C_NUM_1):
  SDA = GPIO7
  SCL = GPIO8
  INT = GPIO (configurable, típicamente 9)
  RST = GPIO (configurable, típicamente 10)
```

**Dirección I2C:** 0x5D (GT911 estándar)

---

## 2. INICIALIZACIÓN

### 2.1 Setup I2C

```cpp
// P4 setup()
Wire1.begin(GPIO7, GPIO8);  // I2C_NUM_1: SDA=7, SCL=8

// Inicializar GT911
GT911_Init();  // Librería específica o custom driver
```

### 2.2 Calibración

GT911 requiere calibración de puntos táctiles:
```
1. Mostrar pantalla de calibración (4 puntos esquinas)
2. Usuario presiona cada punto
3. Driver GT911 aprende mapeo pantalla vs touch controller
4. Guardar calibración en EEPROM GT911
```

---

## 3. GESTOS Y EVENTOS

### 3.1 Eventos Touch

```cpp
// LVGL recibe eventos touch vía indev (input device)
typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t pressure;  // 0-255
    lv_indev_state_t state;  // LV_INDEV_STATE_PRESSED / RELEASED
} lv_indev_data_t;
```

### 3.2 Gestos Soportados

- **Tap** — presión corta
- **Long press** — sostenido >500ms
- **Drag** — movimiento con presión
- **Swipe** — movimiento rápido

---

## 4. LVGL INTEGRATION

### 4.1 Registrar Device Input

```cpp
// P4 setup()
static lv_indev_drv_t indev_drv;
lv_indev_drv_init(&indev_drv);
indev_drv.type = LV_INDEV_TYPE_POINTER;
indev_drv.read_cb = gt911_read_callback;
lv_indev_drv_register(&indev_drv);
```

### 4.2 Callback de Lectura

```cpp
void gt911_read_callback(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    // Leer estado GT911
    uint16_t x, y;
    uint8_t pressed;
    GT911_ReadTouch(&x, &y, &pressed);
    
    // Reporte a LVGL
    data->point.x = x;
    data->point.y = y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
```

---

## 5. TROUBLESHOOTING

### 5.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Touch no funciona | GT911 no encontrado en I2C | Usar i2c_scanner, verificar direccion 0x5D |
| Coordenadas invertidas | Mapeo X/Y al revés | Invertir en callback: `data->point.x = 480 - x` |
| Lag al presionar | INT pin no funciona (polling lento) | Verificar INT pin configurado y generando interrupts |
| Calibración fallida | Touch panel sucio o defectuoso | Limpiar pantalla, comprobar continuidad INT/RST |
| Multi-touch no funciona | Driver GT911 no soporta | Usar librería Adafruit_GT911 |

### 5.2 Debugging

**I2C Scanner:**
```cpp
void i2c_scanner() {
    for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
        if (Wire1.beginTransmission(addr) == 0) {
            Serial.printf("GT911 encontrado en 0x%02X\n", addr);
            Wire1.endTransmission();
        }
    }
}
```

---

## 6. REFERENCIAS

- **DISPLAY_P4.md** — Display ST7701S, LVGL v9, portrait/landscape
- **LVGL Pointer:** https://docs.lvgl.io/9.0/widgets/core/indev.html
- **GT911 datasheet:** Goodix GT911 Capacitive Touch Panel Controller
- **P4 README:** [MASTER_S3-P4/P4/README.md](../MASTER_S3-P4/P4/README.md)

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado TOUCH.md como documento P4-específico, extraído de CLAUDE.md
