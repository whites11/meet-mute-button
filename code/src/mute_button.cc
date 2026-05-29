#include <bsp/board.h>
#include <tusb.h>

#include <hardware/gpio.h>
#include <pico/bootrom.h>

#include "our_descriptor.h"
#include "ws2812.h"

#define BUTTON_PIN 3
#define NUM_PIXELS 16

#define RED 0x001000
#define GREEN 0x100000

bool mute_state = false;
bool oncall_state = false;

void update_led() {
    uint32_t color = 0x000000;
    if (oncall_state) {
        if (mute_state) {
            color = RED;
        } else {
            color = GREEN;
        }
    }

    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(color);
    }
}

int main() {
    board_init();
    tusb_init();
    neopixel_init();

    // Startup flash test: 3x red/green blink
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < NUM_PIXELS; i++) {
            put_pixel(RED);
        }
        sleep_ms(200);
        for (int i = 0; i < NUM_PIXELS; i++) {
            put_pixel(GREEN);
        }
        sleep_ms(200);
    }
    // Turn off after startup test
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(0x000000);
    }

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

/*
    sleep_ms(10);
    if (!gpio_get(BUTTON_PIN)) {
        reset_usb_boot(0, 0);
    }
*/

    update_led();

    uint8_t report;

    const uint32_t DEBOUNCE_US = 50000;
    bool stable_state = false;
    bool last_raw = false;
    absolute_time_t last_change = get_absolute_time();

    while (true) {
        tud_task();
        if (!tud_hid_ready()) {
            continue;
        }

        bool raw = !gpio_get(BUTTON_PIN);
        if (raw != last_raw) {
            last_raw = raw;
            last_change = get_absolute_time();
        }
        if (raw != stable_state &&
            (uint32_t)absolute_time_diff_us(last_change, get_absolute_time()) > DEBOUNCE_US) {
            stable_state = raw;
            if (raw) {
                report = !mute_state;
                tud_hid_report(REPORT_ID, &report, 1);
            }
        }
        sleep_ms(4);
    }

    return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (bufsize >= 1) {
        oncall_state = buffer[0] & 0x01;
        mute_state = buffer[0] & 0x02;
        update_led();
    }
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    if (report_id == REPORT_ID && report_type == HID_REPORT_TYPE_INPUT) {
        buffer[0] = mute_state ? 1 : 0;
        return 1;
    }
    return 0;
}
