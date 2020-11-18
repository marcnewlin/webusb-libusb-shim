#include <stdint.h>
#include <stdbool.h>

bool ensure_navigator_usb();
int request_device_access();
int get_device_descriptor(struct libusb_device_descriptor *desc);
void open_device();
void close_device();
int get_string_descriptor(uint8_t desc_index, uint8_t *data, int length);
int get_configuration();
struct libusb_config_descriptor * get_active_config_descriptor();
void claim_interface(int interface_number);
void release_interface(int interface_number);
int control_transfer(uint8_t request_type, uint8_t bRequest, uint16_t wValue, 
                     uint16_t wIndex, uint8_t *data, uint16_t wLength, unsigned int timeout);
int open_device_with_vid_pid(uint16_t vid, uint16_t pid);
int submit_bulk_in_transfer(uint8_t ep, int length, uint8_t * buffer, struct libusb_transfer *transfer);
