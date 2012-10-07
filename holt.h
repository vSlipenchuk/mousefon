#ifndef HOLT_H
#define HOLT_H

#include <stdint.h>
#include <linux/input.h> //event type, keycodes

#define HOLT_HID_VERSION 0x10004

#define HOLT_VENDOR_ID 0x04b4
#define HOLT_DEVICE_ID 0x0407
#define HOLT_DEVICE_VERSION 0x143
#define HOLT_NUM_APPLICATIONS 2

#define HOLT_DEFAULT_DEVICE "/dev/usb/hiddev0"

#define HOLT_LCD_WIDTH 128
#define HOLT_LCD_HEIGHT 64
#define HOLT_LCD_BYTEMAP_SIZE (HOLT_LCD_HEIGHT * HOLT_LCD_WIDTH)
#define HOLT_LCD_BITMAP_SIZE (HOLT_LCD_BYTEMAP_SIZE / 8)

int holt_init (char *filename);
int holt_fd (void);
void holt_deinit (void);

void holt_read_event (struct input_event *ev);
void holt_lcd_clear (void);
void holt_lcd_bitmap (const uint8_t *bitmap);
void holt_lcd_tux (void);
void holt_lcd_convert_bitmap (const uint8_t *bits_in, uint8_t *bits_out);
void holt_lcd_convert_bytemap (const uint8_t *bytes_in, uint8_t *bits_out);


/* Internal use: */
#define HOLT_COMMAND_SOUND 1     //turn speaker/external headphone on/off
#define HOLT_COMMAND_DRAW 3      //draw stuff onto LCD display
#define HOLT_COMMAND_BACKLIGHT 4 //turn backlight on/off
#define HOLT_COMMAND_STATUS 5    //request status

#define HOLT_BIT_SPEAKER 1
#define HOLT_BIT_HEADPHONE 2
#define HOLT_BIT_BACKLIGHT 1

#define HOLT_INPUT_USAGE_SIZE_1 7 //one 7-byte field
#define HOLT_INPUT_USAGE_SIZE_2 6 //HID reports 6 (1-byte) fields
#define HOLT_INPUT_USAGE_SIZE_MAX 7
#define HOLT_OUTPUT_USAGE_SIZE 17

/* Internal pipe commands */
#define HOLT_EXIT 1 

/* Status bits for holt_status */
#define HOLT_STATUS_OPEN      1 //Mouse opened
#define HOLT_STATUS_HEADPHONE 2 //External headphone plugged in

extern int holt_fd_hiddev;
extern unsigned int holt_status;

/* Internal functions */

/* misc.c */
int holt_write_all (int fd, void *buf, size_t size);
int holt_write_event (int fd, struct input_event *ev);
int holt_read_all (int fd, void *buf, size_t size);

/* report.c */
int holt_read_report (int fd_user);
void holt_write_report (unsigned char *report);
void holt_set_sound (int speaker, int headphone);
void holt_set_backlight (int on);
void holt_request_status (void);

#endif
