#pragma once

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <optional>
#include <vector>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include <esp_log.h>
#include <esp_system.h>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>

#include <soc/gpio_struct.h>

void reboot(); // Delayed reboot

inline TickType_t s2ticks(uint32_t time) {return time * 1000 / portTICK_PERIOD_MS;}
inline TickType_t ms2ticks(uint32_t time) {return time && time < portTICK_PERIOD_MS ? 1 : time / portTICK_PERIOD_MS;}

template<class ... Args>
inline constexpr uint32_t bit(Args ... args) { return ((1 << int(args)) | ... );}
