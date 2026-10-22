/*
 * usb_descriptors.c — single HID keyboard interface, boot protocol.
 *
 * VID 0xCAFE (private), PID from board_config.h, product string from
 * HID_KEYBOARD_NAME. Callbacks: device / configuration / string / HID report
 * descriptors plus get/set-report and set-protocol stubs; mount/unmount
 * callbacks are intentionally empty (mount state is read live from the stack
 * via tud_mounted()).
 */
#include <string.h>

#include "tusb.h"

#include "usb_descriptors.h"
#include "board_config.h"

enum {
  ITF_NUM_KEYBOARD = 0,
  ITF_NUM_TOTAL    = 1,
};

#define EPNUM_KEYBOARD 0x81

uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD()
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

uint8_t const desc_configuration[] = {
  /* Config: 1 config, 1 interface, no string, remote-wakeup, 100 mA. */
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  /* Keyboard interface, boot protocol, interrupt IN endpoint, 10 ms poll. */
  TUD_HID_DESCRIPTOR(ITF_NUM_KEYBOARD, 0, HID_ITF_PROTOCOL_KEYBOARD,
                     sizeof(desc_hid_report), EPNUM_KEYBOARD,
                     CFG_TUD_HID_EP_BUFSIZE, 10),
};

tusb_desc_device_t const desc_device = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor           = KEYSTROKER_USB_VID,
  .idProduct          = KEYSTROKER_USB_PID,
  .bcdDevice          = 0x0100,
  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,
  .bNumConfigurations = 0x01,
};

/* String descriptors: 0 = lang IDs, 1 = manufacturer, 2 = product, 3 = serial. */
static const char *const string_desc_arr[] = {
  (const char[]) { 0x09, 0x04 }, /* 0: English (0x0409) */
  "Arboard",                    /* 1 */
  HID_KEYBOARD_NAME,            /* 2 */
  KEYSTROKER_USB_SERIAL,        /* 3 */
};

static uint16_t _desc_str[32];

/* Invoked when the host requests the device descriptor. */
uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&desc_device;
}

/* Invoked when the host requests the configuration descriptor. */
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return desc_configuration;
}

/* Invoked when the host requests a string descriptor. */
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  uint8_t chr_count;

  if (index == 0) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else {
    if (index >= (uint8_t)(sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
      return NULL;
    }
    const char *str = string_desc_arr[index];
    chr_count = (uint8_t)strlen(str);
    if (chr_count > 31) {
      chr_count = 31;
    }
    for (uint8_t i = 0; i < chr_count; i++) {
      _desc_str[1 + i] = (uint16_t)str[i];
    }
  }

  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2u * chr_count + 2u));
  return _desc_str;
}

/* Invoked when the host requests the HID report descriptor. */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  (void)instance;
  return desc_hid_report;
}

/* Host GET_REPORT (boot keyboards rarely use it): stall by returning 0. */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;
  return 0;
}

/* Host SET_REPORT (e.g. Caps Lock LED state): accepted, LEDs ignored. */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)bufsize;
}

/* Accept boot/report protocol switches without action. */
void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {
  (void)instance;
  (void)protocol;
}

/* Mount state is read live via tud_mounted(); hooks kept for status LEDs. */
void tud_mount_cb(void) {
}

void tud_umount_cb(void) {
}
