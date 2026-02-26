#include "drawing.h"
#include "esp_log.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

char *number_str(int number) {
  int a = number / 10;
  int b = number - a * 10;

  char *num = malloc(3);

  num[1] = b + '0';
  num[0] = a + '0';
  num[2] = '\0';
  return num;
}

char *year_to_char(int year) {
  char *y = malloc(5);
  if (!y)
    return NULL;

  y[0] = (year / 1000) % 10 + '0';
  y[1] = (year / 100) % 10 + '0';
  y[2] = (year / 10) % 10 + '0';
  y[3] = year % 10 + '0';
  y[4] = '\0';

  return y;
}


void draw_layout_change(int hours, int minutes, int seconds, int year, int day,
                        int month, char *mode, int c) {
  char *hours_str = number_str(hours);
  char *minute_str = number_str(minutes);
  char *second_str = number_str(seconds);
  char *year_str = year_to_char(year);
  char *day_str = number_str(day);
  char *month_str = number_str(month);

  ESP_LOGI("I", "%s", hours_str);

  draw_string(1, 1, year_str, 1, 1);
  draw_string(25, 1, "/", 1, 1);
  draw_string(30, 1, month_str, 1, 1);
  draw_string(42, 1, "/", 1, 1);
  draw_string(47, 1, day_str, 1, 1);

  draw_string(75, 1, mode, 1, 1);

  ssd1306_draw_horizental_line(0, 18, 45, 35, c == 0 ? 1 : 0);
  draw_string(1, 20, hours_str, 4, c == 0 ? 0 : 1);
  
  draw_string(45, 20, ":", 2, 1);
  
  ssd1306_draw_horizental_line(53, 18, 35, 35, c == 1 ? 1 : 0);
  draw_string(55, 20, minute_str, 3, c == 1 ? 0 : 1);
  
  draw_string(87, 20, ":", 2, 1);
  
  ssd1306_draw_horizental_line(92, 18, 35, 35, c == 2 ? 1 : 0);
  draw_string(95, 20, second_str, 3, c == 2 ? 0 : 1);

  free(hours_str);
  free(minute_str);
  free(second_str);

  free(year_str);
  free(month_str);
  free(day_str);
}
