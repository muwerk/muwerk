#include <stdio.h>

#define USE_SERIAL_DBG 1

#define __RP2040__
#include "ustd_platform.h"

#include "scheduler.h"
ustd::Scheduler sched;

int main() {
    //    setup_default_uart();
    stdio_init_all();
    const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    while (true) {
        printf("Hello, world!\n");
        sleep_ms(750);
        gpio_put(LED_PIN, 1);
        sleep_ms(250);
        gpio_put(LED_PIN, 0);
    }
    return 0;
}
