// src/display/Display.cpp
#include "Display.h"
#include "../config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "lcd/esp_lcd_st7701.h"
#include "touch/esp_lcd_touch_gt911.h"
#include "lvgl.h"

#define LCD_H_RES           480
#define LCD_V_RES           800
#define LCD_RST_PIN         GPIO_NUM_5
#define LCD_BACKLIGHT_PIN   GPIO_NUM_23
#define LCD_LEDC_CHANNEL    LEDC_CHANNEL_0
#define MIPI_DSI_PHY_LDO_CHAN   3
#define MIPI_DSI_PHY_LDO_MV     2500

static esp_lcd_panel_handle_t s_panel = NULL;
static lv_display_t* s_disp = NULL;

static lv_obj_t* s_root         = NULL;
static lv_obj_t* s_content_area = NULL;

lv_obj_t* displayGetRoot()        { return s_root; }
lv_obj_t* displayGetContentArea() { return s_content_area; }


static void backlight_init() {
    const ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_1,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    const ledc_channel_config_t channel_cfg = {
        .gpio_num   = LCD_BACKLIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LCD_LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_1,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_timer_config(&timer_cfg);
    ledc_channel_config(&channel_cfg);
}

void displaySetBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint32_t duty = (1023 * percent) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL);
}

static void init_mipi_dsi_power() {
    static esp_ldo_channel_handle_t ldo_handle = NULL;
    esp_ldo_channel_config_t cfg = {
        .chan_id    = MIPI_DSI_PHY_LDO_CHAN,
        .voltage_mv = MIPI_DSI_PHY_LDO_MV,
    };
    esp_ldo_acquire_channel(&cfg, &ldo_handle);
}

lv_display_t* getDisplay() { return s_disp; }

void initDisplay() {
    backlight_init();
    init_mipi_dsi_power();

    // DSI bus
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id             = 0,
        .num_data_lanes     = 2,
        .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 1000,
    };
    esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus);

    // DBI IO
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_dbi_io_config_t dbi_cfg = ST7701_PANEL_IO_DBI_CONFIG();
    esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, &io);

    // DPI config
    esp_lcd_dpi_panel_config_t dpi_cfg =
        ST7701_480_360_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    dpi_cfg.num_fbs = 2;

    // Panel
    st7701_vendor_config_t vendor_cfg = {
        .init_cmds      = NULL,
        .init_cmds_size = 0,
        .mipi_config = {
            .dsi_bus    = dsi_bus,
            .dpi_config = &dpi_cfg,
        },
        .flags = { .use_mipi_interface = 1 },
    };
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order  = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
        .vendor_config  = &vendor_cfg,
    };
    esp_lcd_new_panel_st7701(io, &panel_cfg, &s_panel);
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);

    // I2C para GT911
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port          = I2C_NUM_1,
        .sda_io_num        = GPIO_NUM_7,
        .scl_io_num        = GPIO_NUM_8,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };
    i2c_new_master_bus(&i2c_cfg, &i2c_bus);

    // GT911
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_cfg.scl_speed_hz = 400000;
    esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io);

    esp_lcd_touch_config_t tp_cfg = {
        .x_max        = LCD_H_RES,
        .y_max        = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels       = {.reset = 0, .interrupt = 0},
        .flags        = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
    };
    static esp_lcd_touch_handle_t s_tp = NULL;
    esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_tp);

    // LVGL init
    lv_init();

    // Buffers
    static uint8_t* lvgl_buf1 = (uint8_t*)heap_caps_malloc(
        LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    static uint8_t* lvgl_buf2 = (uint8_t*)heap_caps_malloc(
        LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    s_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_buffers(s_disp, lvgl_buf1, lvgl_buf2,
                           LCD_H_RES * 100 * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, [](lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
        esp_lcd_panel_draw_bitmap(s_panel,
                                  area->x1, area->y1,
                                  area->x2 + 1, area->y2 + 1,
                                  px_map);
        lv_display_flush_ready(disp);
    });

   // LVGL input device touch
lv_indev_t* indev = lv_indev_create();
lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(indev, [](lv_indev_t* drv, lv_indev_data_t* data) {
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_user_data(drv);
    uint16_t x, y, strength;
    uint8_t count = 0;
    esp_lcd_touch_read_data(tp);
    if (esp_lcd_touch_get_coordinates(tp, &x, &y, &strength, &count, 1) && count > 0) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
});
lv_indev_set_user_data(indev, s_tp);
   
    
    // ── Pantalla raíz ────────────────────────────────────────────────
s_root = lv_obj_create(NULL);
lv_obj_set_style_bg_color(s_root, lv_color_hex(COL_BG), 0);
lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
lv_obj_set_style_pad_all(s_root, 0, 0);
lv_obj_set_style_border_width(s_root, 0, 0);
lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLL_ELASTIC);
lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);  // ← añadir
lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLL_CHAIN_VER);  // ← añadir

// ── Content area — padre de todas las páginas ────────────────────
s_content_area = lv_obj_create(s_root);
lv_obj_set_pos(s_content_area, 0, 0);
lv_obj_set_size(s_content_area, HEADER_X, P4_H);
lv_obj_set_style_pad_all(s_content_area, 0, 0);
lv_obj_set_style_border_width(s_content_area, 0, 0);
lv_obj_set_style_bg_color(s_content_area, lv_color_hex(COL_BG), 0);
lv_obj_set_style_bg_opa(s_content_area, LV_OPA_COVER, 0);
lv_obj_clear_flag(s_content_area, LV_OBJ_FLAG_SCROLLABLE);
lv_obj_clear_flag(s_content_area, LV_OBJ_FLAG_SCROLL_ELASTIC);
lv_obj_clear_flag(s_content_area, LV_OBJ_FLAG_SCROLL_MOMENTUM);
lv_obj_clear_flag(s_content_area, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);  // ← añadir
lv_obj_clear_flag(s_content_area, LV_OBJ_FLAG_SCROLL_CHAIN_VER);  // ← añadir
lv_obj_add_flag(s_content_area, LV_OBJ_FLAG_CLICKABLE);

lv_scr_load(s_root);
    log_i("[Display] Init OK");
}
