#ifndef DISPLAY_H
#define DISPLAY_H
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "lvgl/lvgl.h"
#include "lvgl_demo_ui.h"



#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (6528000) //(10 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

#define EXAMPLE_PIN_NUM_DATA0 39
#define EXAMPLE_PIN_NUM_DATA1 40
#define EXAMPLE_PIN_NUM_DATA2 41
#define EXAMPLE_PIN_NUM_DATA3 42
#define EXAMPLE_PIN_NUM_DATA4 45
#define EXAMPLE_PIN_NUM_DATA5 46
#define EXAMPLE_PIN_NUM_DATA6 47
#define EXAMPLE_PIN_NUM_DATA7 48

#define EXAMPLE_PIN_RD GPIO_NUM_9
#define EXAMPLE_PIN_PWR 15
#define EXAMPLE_PIN_NUM_PCLK GPIO_NUM_8 // LCD_WR
#define EXAMPLE_PIN_NUM_CS 6
#define EXAMPLE_PIN_NUM_DC 7
#define EXAMPLE_PIN_NUM_RST 5
#define EXAMPLE_PIN_NUM_BK_LIGHT 38

// The pixel number in horizontal and vertical
#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 170
#define LVGL_LCD_BUF_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES)
// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS 8
#define EXAMPLE_LCD_PARAM_BITS 8

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

static void example_increase_lvgl_tick(void *arg);
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void main_creatSysteTasks(void);
static void lvglTimerTask(void *param);
void display_init(void);

#endif