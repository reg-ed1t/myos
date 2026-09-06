#ifndef drivers_h
#define drivers_h

#include <stdint.h>

#define CMOS_ADDRESS_PORT 0x70
#define CMOS_DATA_PORT    0x71
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL2_PORT 0x42
#define SPEAKER_PORT     0x61
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64
#define PS2_DATA_PORT    0x60

extern volatile char command_buffer[64];
extern volatile int command_len;
extern volatile uint8_t command_ready;
extern volatile uint32_t timer_ticks;

// lower case keys table
extern const char scancode_to_ascii[];

void init_timer(uint32_t frequency);
void timer_handler();
void sleep(uint32_t ticks);
void keyboard_handler();

extern uint8_t rtc_second;
extern uint8_t rtc_minute;
extern uint8_t rtc_hour;
extern uint8_t rtc_day;
extern uint8_t rtc_month;
extern uint32_t rtc_year;

void read_rtc(void);
void play_sound(uint32_t frequency);
void stop_sound(void);
void beep(uint32_t frequency, uint32_t duration_ticks);

extern volatile int32_t mouse_x;
extern volatile int32_t mouse_y;
extern volatile uint8_t mouse_buttons;

void init_mouse(void);

#endif