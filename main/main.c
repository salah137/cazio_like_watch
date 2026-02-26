#include "drawing.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/gptimer_types.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/timer_types.h"
#include "i2c_connection.h"
#include "portmacro.h"
#include "soc/clk_tree_defs.h"
#include "utilitys.h"
#include <inttypes.h>
#include <stdbool.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define TOP_RIGHT 27
#define TOP_LEFT 12
#define BOTTOM_RIGHT 14
#define BOTTOM_LEFT 13
#define LED_ALARM_PIN 5

enum modes { WATCH, STOP_WATCH, RUNNING_STOP, CHANGE_TIMING, SET_ALARM };

static int hours = 0;
static int minutes = 0;
static int seconds = 0;

static int day = 10;
static int month = 10;
static int year = 2025;

static int stop_hours = 0;
static int stop_minutes = 0;
static int stop_seconds = 0;

static int alarm_h = 0;
static int alarm_m = 0;
static int alarm_s = 0;

static int set_up[3];

static int set_up_pin = 0;
volatile int lastPush;

static volatile bool change = true;
static int mode = WATCH;
static TaskHandle_t calc_task_handle;
TaskHandle_t watch_handle;

QueueHandle_t dataQueue;

static bool alarm_on = false;

void add_second() {
  seconds++;
  if (seconds == 60) {
    seconds = 0;
    minutes++;
    if (minutes == 60) {
      minutes = 0;
      hours++;
      if (hours == 24) {
        hours = 0;
        day++;
        if (month == 1 || month == 3 || month == 5 || month == 7 ||
            month == 8 || month == 10 || month == 12) {
          if (day == 31) {
            day = 1;
            month++;
            if (month == 13) {
              month = 1;
              year++;
            }
          }
        } else if (month == 2) {
          if (day == 28) {
            day = 1;
            month++;
          }
        } else {
          if (day == 30) {
            day = 1;
            month++;
          }
        }
      }
    }
  }
  if (alarm_h == hours && alarm_m == minutes && alarm_s == seconds &&
      alarm_on) {
    gpio_set_level(LED_ALARM_PIN, 0);
  }
}

void add_stop() {
  stop_seconds++;
  if (stop_seconds == 60) {
    stop_seconds = 0;
    stop_minutes++;
    if (stop_minutes == 60) {
      stop_minutes = 0;
      stop_hours++;
    }
  }
}



uint32_t millis() { return (xTaskGetTickCount() * portTICK_PERIOD_MS); }

void add_to_set_up() {
  set_up[set_up_pin]++;
  if (set_up_pin == 2 || set_up_pin == 1) {
    if (set_up[set_up_pin] == 60)
      set_up[set_up_pin] = 0;
  } else {
    if (set_up[set_up_pin] == 24)
      set_up[set_up_pin] = 0;
  }
}

void minus_to_set_up() {
  set_up[set_up_pin]--;
  if (set_up[set_up_pin] == -1) {
    if (set_up_pin == 1 || set_up_pin == 2) {
      set_up[set_up_pin] = 59;
    } else {
      set_up[set_up_pin] = 23;
    }
  }
}

void handle_input() {
  int pressed_btn;
  while (1) {
    if (xQueueReceive(dataQueue, &pressed_btn, 0) == pdTRUE) {
      if (!gpio_get_level(LED_ALARM_PIN)) {
        gpio_set_level(LED_ALARM_PIN, 1);
      } else {
        if (pressed_btn == TOP_RIGHT) {
          ESP_LOGI("=>", "pressed TOP RIGHt");

          if (mode == WATCH || mode == STOP_WATCH) {
            mode = RUNNING_STOP;

            stop_hours = 0;
            stop_minutes = 0;
            stop_seconds = 0;
          } else if (mode == CHANGE_TIMING || mode == SET_ALARM) {
            add_to_set_up();
          }

        } else if (pressed_btn == BOTTOM_LEFT) {
          ESP_LOGI("=>", "pressed BOTTOM LEFT");

          if (mode == CHANGE_TIMING) {
            hours = set_up[0];
            minutes = set_up[1];
            seconds = set_up[2];
          } else if (mode == SET_ALARM) {
            alarm_h = set_up[0];
            alarm_m = set_up[1];
            alarm_s = set_up[2];
          }
          mode = WATCH;
        } else if (pressed_btn == BOTTOM_RIGHT) {
          ESP_LOGI("=>", "pressed BOTTOM RIGHT");
          if (mode == RUNNING_STOP) {
            mode = STOP_WATCH;
                 } else if (mode == CHANGE_TIMING) {
            minus_to_set_up();
          } else if (mode == WATCH) {
            set_up[0] = alarm_h;
            set_up[1] = alarm_m;
            set_up[2] = alarm_s;

            mode = SET_ALARM;
          } else if (mode == SET_ALARM) {
            ESP_LOGI("=>", "change alarm on");
            alarm_on = !alarm_on;
          }

        } else if (pressed_btn == TOP_LEFT) {
          ESP_LOGI("=>", "pressed TOP LEFT");

          if (mode == STOP_WATCH) {
            mode = RUNNING_STOP;
          } else if (mode == WATCH) {
            set_up[0] = hours;
            set_up[1] = minutes;
            set_up[2] = seconds;

            mode = CHANGE_TIMING;
          } else if (mode == CHANGE_TIMING || mode == SET_ALARM) {
            set_up_pin++;
            if (set_up_pin == 3) {
              set_up_pin = 0;
            }
          }
        }

        pressed_btn = -1;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void watch_task(void *args) {
  screen_t oled;
  init(SDA_PIN, SCL_PIN, &oled);

  while (1) {
    if (mode == WATCH) {
      if (change) {
        ssd1306_clear_fb();
        draw_layout_change(hours, minutes, seconds, year, day, month, "WATCH",
                           -1);
        ssd1306_update(&oled);
        change = false;
      }
    } else if (mode == STOP_WATCH || mode == RUNNING_STOP) {
      if (change) {
        ssd1306_clear_fb();
        draw_layout_change(stop_hours, stop_minutes, stop_seconds, year, day,
                           month, "STP WATCH", -1);
        ssd1306_update(&oled);
        change = false;
      }
    } else if (mode == CHANGE_TIMING) {
      ssd1306_clear_fb();

      draw_layout_change(set_up[0], set_up[1], set_up[2], year, day, month,
                         "SET UP", set_up_pin);
      ssd1306_update(&oled);
    } else if (mode == SET_ALARM) {

      ssd1306_clear_fb();

      draw_layout_change(set_up[0], set_up[1], set_up[2], year, day, month,
                         alarm_on ? "ALARM ON" : "ALARM OFF", set_up_pin);
      ssd1306_update(&oled);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void calculate_time(void *args) {
  calc_task_handle = xTaskGetCurrentTaskHandle();

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    add_second();
    if (mode == RUNNING_STOP)
      add_stop();
    change = true;
  }
}

static bool IRAM_ATTR alarm_cb(gptimer_handle_t timer,
                               const gptimer_alarm_event_data_t *edata,
                               void *user_ctx) {
  BaseType_t hpw = pdFALSE;

  xTaskNotifyFromISR(calc_task_handle, 0, eNoAction, &hpw);
  return hpw == pdTRUE;
}

static void IRAM_ATTR isr_handler(void *args) {
  int current = millis();

  if (lastPush == 0 || current - lastPush > pdMS_TO_TICKS(100)) {
    lastPush = current;
    int pressed = (int)args;
    BaseType_t hp = pdFALSE;
    xQueueSendFromISR(dataQueue, &pressed, &hp);
    if (hp) {
      portYIELD_FROM_ISR();
    }
  }
}


gpio_config_t config_for_intr(uint64_t pin) {
  gpio_config_t io_config = {.pin_bit_mask = 1ULL << pin,
                             .mode = GPIO_MODE_INPUT,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .pull_up_en = GPIO_PULLUP_ENABLE,
                             .intr_type = GPIO_INTR_NEGEDGE};
  return io_config;
}

void app_main(void) {
  dataQueue = xQueueCreate(10, sizeof(int));

  gptimer_handle_t gptimer = NULL;

  gptimer_config_t gptimer_config = {.clk_src = GPTIMER_CLK_SRC_DEFAULT,
                                     .direction = GPTIMER_COUNT_UP,
                                     .resolution_hz = 1000 * 1000};

  ESP_ERROR_CHECK(gptimer_new_timer(&gptimer_config, &gptimer));

  gptimer_alarm_config_t alarm_config = {
      .alarm_count = 1000000,
      .reload_count = 0,
      .flags.auto_reload_on_alarm = true,
  };

  ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

  gptimer_event_callbacks_t alarm_cb_cf = {
      .on_alarm = alarm_cb,
  };

  ESP_ERROR_CHECK(
      gptimer_register_event_callbacks(gptimer, &alarm_cb_cf, NULL));

  ESP_ERROR_CHECK(gptimer_enable(gptimer));
  ESP_ERROR_CHECK(gptimer_start(gptimer));

  gpio_config_t top_right = config_for_intr(TOP_RIGHT);
  gpio_config_t top_left = config_for_intr(TOP_LEFT);
  gpio_config_t bottom_right = config_for_intr(BOTTOM_RIGHT);
  gpio_config_t bottom_left = config_for_intr(BOTTOM_LEFT);

  gpio_config(&top_right);
  gpio_config(&top_left);
  gpio_config(&bottom_left);
  gpio_config(&bottom_right);

  gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

  gpio_isr_handler_add(TOP_RIGHT, isr_handler, (void *)TOP_RIGHT);
  gpio_isr_handler_add(BOTTOM_RIGHT, isr_handler, (void *)BOTTOM_RIGHT);
  gpio_isr_handler_add(TOP_LEFT, isr_handler, (void *)TOP_LEFT);
  gpio_isr_handler_add(BOTTOM_LEFT, isr_handler, (void *)BOTTOM_LEFT);

  gpio_reset_pin(LED_ALARM_PIN);
  gpio_set_direction(LED_ALARM_PIN, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level(LED_ALARM_PIN, 1);

  xTaskCreate(watch_task, "watch_task", 4096, NULL, 5, &watch_handle);
  xTaskCreate(calculate_time, "time_calc", 2048, NULL, 7, NULL);
  xTaskCreate(handle_input, "handle_inpt", 2048, NULL, 6, NULL);
}
