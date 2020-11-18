#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


EM_JS(bool, ensure_navigator_usb, (), {
  return navigator.usb !== undefined;
});


EM_JS(int, get_device_descriptor, (struct libusb_device_descriptor *desc), {
  return _get_device_descriptor(desc);
});


EM_JS(void, open_device, (), {
  return _open_device();
});


EM_JS(int, open_device_with_vid_pid, (uint16_t vid, uint16_t pid), {
  return _open_device_by_vid_pid(vid, pid);
});


EM_JS(void, close_device, (), {
  return _close_device();
});


EM_JS(int, get_configuration, (), {
  return _get_configuration();
});


EM_JS(int, get_string_descriptor, (uint8_t desc_index, uint8_t *data, int length), {
  return _get_string_descriptor(desc_index, data, length);
});


EM_JS(struct libusb_config_descriptor *, get_active_config_descriptor, (), {
  return _get_active_config_descriptor();
});


EM_JS(void, claim_interface, (int interface_number), {
  return _claim_interface(interface_number);
});


EM_JS(void, release_interface, (int interface_number), {
  return _release_interface(interface_number);
});


EM_JS(int, control_transfer, (uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	                            uint8_t *data, uint16_t wLength, unsigned int timeout), {
  return _control_transfer(request_type, bRequest, wValue, wIndex, data, wLength, timeout);
});


int submit_bulk_in_transfer(uint8_t ep, int length, uint8_t * buffer, struct libusb_transfer *transfer){
  return MAIN_THREAD_EM_ASM_INT({ return _submit_bulk_in_transfer($0, $1, $2, $3); }, ep, length, buffer, transfer);
};


int request_device_access() {

  // get the configured VID/PID from the main thread
  uint16_t vid = MAIN_THREAD_EM_ASM_INT({ return _get_vid(); });
  uint16_t pid = MAIN_THREAD_EM_ASM_INT({ return _get_pid(); });

  // request the USB device from the current thread
  return open_device_with_vid_pid(vid, pid);
}