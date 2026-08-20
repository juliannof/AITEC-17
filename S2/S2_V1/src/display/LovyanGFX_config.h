// src/display/LovyanGFX_config.h
#pragma once
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host    = SPI3_HOST;
            cfg.spi_mode    = 0;
            // SPI a 80MHz — máximo típico ESP32+ST7789 (2026-08-20, confirmado
            // tras diagnóstico). Bajado a 40MHz temporalmente por saltos de
            // posición físicamente imposibles del fader — CAUSA REAL: unidad
            // buck defectuosa en la ATX del banco (rail 5V, lógica), no el SPI.
            // Sustituida la unidad buck → problema desaparece. Ver CHANGELOG
            // sesión 2026-08-20 17:39. fillScreen() ~110ms a 10MHz → ~14ms aquí.
            cfg.freq_write  = 80000000;
            cfg.freq_read   = 20000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 7;
            cfg.pin_mosi    = 4;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 6;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 5;
            cfg.pin_rst          = 33;
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;
            cfg.panel_width      = 240;
            cfg.panel_height     = 280;
            cfg.offset_x         = 0;
            cfg.offset_y         = 20;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = 3;
            cfg.invert      = false;
            cfg.freq        = 500;
            cfg.pwm_channel = 0;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};