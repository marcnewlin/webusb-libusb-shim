#include <emscripten.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb.h>

#include "webusb.h"

static bool enable_debug_log = false;

void debug_log (char *fmt, ...) 
{
  if(enable_debug_log) {
    va_list argp;
    va_start (argp, fmt); 
    vfprintf (stderr, fmt, argp); 
    fprintf(stderr, "\n");
    va_end (argp);
  }
}

// override sleep(...) to use emscripten_sleep(...) instead
unsigned int sleep(unsigned int seconds)
{
  emscripten_sleep(seconds*1000);
}


/*************************************************
 * simple linked-list to track pending transfers *
 *************************************************/

struct pending_transfer {
  struct libusb_transfer * transfer;
  struct pending_transfer * next;
  struct pending_transfer * previous;
};

struct pending_transfer * pending_transfers = NULL;

void add_pending_transfer(struct libusb_transfer * t)
{
  if(pending_transfers == NULL) pending_transfers = calloc(1, sizeof(struct pending_transfer));

  struct pending_transfer * transfer = calloc(1, sizeof(struct pending_transfer));
  transfer->transfer = t;

  // find the end of the list
  struct pending_transfer * p = pending_transfers;
  while(p->next != NULL) {
    p = p->next;
  } 

  // add this transfer to the list
  transfer->previous = p;
  p->next = transfer;
}

int process_completed_transfers()
{
  // traverse the list for pending transfers
  struct pending_transfer * p = pending_transfers;
  while(p->next != NULL) {
    p = p->next;
    if(p->transfer->status == LIBUSB_TRANSFER_COMPLETED || 
       p->transfer->status == LIBUSB_TRANSFER_CANCELLED) {
      p->transfer->callback(p->transfer);
      p->transfer->status = -1;
    }
  }
  return LIBUSB_SUCCESS;
}




/***************************************
 * libusb API [partial] implementation *
 ***************************************/


// fixed values for context and device handles
const libusb_context * DEFAULT_LIBUSB_CONTEXT = 0; // Fixed context handle
const libusb_device  * DEFAULT_LIBUSB_DEVICE  = 1; // Fixed device handle
#define DEFAULT_BUS_NUMBER     0 // Fixed USB bus number
#define DEFAULT_DEVICE_ADDRESS 0 // Fixed USB device address


int libusb_init(libusb_context **ctx)
{
  debug_log("libusb_init(...)");
  if(!ensure_navigator_usb()) return LIBUSB_ERROR_NOT_SUPPORTED;
  if(ctx != NULL) *ctx = DEFAULT_LIBUSB_CONTEXT;
  return LIBUSB_SUCCESS;
}


ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***list)
{
  debug_log("libusb_get_device_list(...)");

  // validate the context
  if(ctx != DEFAULT_LIBUSB_CONTEXT) return LIBUSB_ERROR_INVALID_PARAM;

  // request access to the configured device
  if(request_device_access() == 0) {
    fprintf(stderr, "request_device_access() USB device not found/authorized\n");
    return LIBUSB_ERROR_NO_DEVICE;
  }

  // generate a two-device list
  // - our target device
  // - dummy NULL device
  libusb_device ** l = malloc(sizeof(libusb_device *) * 2);
  l[0] = DEFAULT_LIBUSB_DEVICE;
  l[1] = NULL;
  *list = l;

  // return the device count
  return 1;
}


int libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *desc)
{
  debug_log("libusb_get_device_descriptor(...)");

  // validate the device
  if(dev != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // generate the device descriptor and write it to the heap
  get_device_descriptor(desc);

  return 0;
}


void libusb_free_device_list(libusb_device **list, int unref_devices)
{
  debug_log("libusb_free_device_list(...)");
  free(list);
}


void libusb_exit(libusb_context *ctx)
{
  debug_log("libusb_exit(...)");
  // nop
}


uint8_t libusb_get_bus_number(libusb_device *dev)
{
  debug_log("libusb_get_bus_number(...)");

  // validate the device
  if(dev != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  return DEFAULT_BUS_NUMBER;
}


uint8_t libusb_get_device_address(libusb_device *dev)
{
  debug_log("libusb_get_device_address(...)");

  // validate the device
  if(dev != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  return DEFAULT_DEVICE_ADDRESS;
}


int libusb_get_port_numbers(libusb_device *dev, uint8_t *port_numbers, int port_numbers_len)
{
  debug_log("libusb_get_port_numbers(...)");

  // validate the device
  if(dev != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // return 0 port numbers
  return 0;
}


int libusb_open(libusb_device *dev, libusb_device_handle **dev_handle)
{
  debug_log("libusb_open(...)");

  // validate the device
  if(dev != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // open the device
  open_device();

  // set the device handle to the device value
  *dev_handle = dev;

  return LIBUSB_SUCCESS;  
}


void libusb_close(libusb_device_handle *dev_handle)
{
  debug_log("libusb_close(...)");

  // validate the device handle
  if(dev_handle != DEFAULT_LIBUSB_DEVICE) return;

  // close the device
  close_device();
}



int libusb_get_string_descriptor_ascii(libusb_device_handle *dev_handle,
	uint8_t desc_index, unsigned char *data, int length)
{
  debug_log("libusb_get_string_descriptor_ascii(...)");

  // validate the device handle
  if(dev_handle != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // get the descriptor string and return the length
  return get_string_descriptor(desc_index, data, length);  
}


int libusb_get_configuration(libusb_device_handle *dev, int *config)
{
  debug_log("libusb_get_configuration(...)");

  // validate the device
  if(dev != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // get the configuration value from WebUSB
  *config = get_configuration();

  return LIBUSB_SUCCESS;
}


libusb_device * libusb_get_device(libusb_device_handle *dev_handle)
{
  debug_log("libusb_get_device(...)");

  // validate the device handle
  if(dev_handle != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // return the fixed device ptr (same as the handle)
  return DEFAULT_LIBUSB_DEVICE;
}


int libusb_get_active_config_descriptor(libusb_device *dev, struct libusb_config_descriptor **config)
{
  debug_log("libusb_get_active_config_descriptor(...)");

  // validate the device
  if(dev != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // serialize the descriptor to the heap
  *config = get_active_config_descriptor();

  return LIBUSB_SUCCESS;  
}


void libusb_free_config_descriptor( struct libusb_config_descriptor *config)
{
  debug_log("libusb_free_config_descriptor(...)");
  free(config);
}


int libusb_kernel_driver_active(libusb_device_handle *dev_handle, int interface_number)
{
  debug_log("libusb_kernel_driver_active(...)");

  // validate the device handle
  if(dev_handle != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // WebUSB only has access to devices which aren't currently
  // owned by the kernel, so we always return 0 (not active)
  return 0;
}


int libusb_claim_interface(libusb_device_handle *dev_handle, int interface_number)
{
  debug_log("libusb_claim_interface(...)");

  // validate the device handle
  if(dev_handle != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // claim the interface
  claim_interface(interface_number);
  
  return LIBUSB_SUCCESS;
}


int libusb_release_interface(libusb_device_handle *dev_handle, int interface_number)
{
  debug_log("libusb_release_interface(...)");

  // validate the device handle
  if(dev_handle != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // release the interface
  release_interface(interface_number);
  
  return LIBUSB_SUCCESS;
}


struct libusb_transfer * libusb_alloc_transfer(int iso_packets)
{
  debug_log("libusb_alloc_transfer(...)");

  // compute the size of the struct give the number of iso packets
  int size = sizeof(struct libusb_transfer) + 
             sizeof(struct libusb_iso_packet_descriptor) * iso_packets;

  // allocate and return
  struct libusb_transfer * xfer = malloc(size);
  return xfer;
}


int libusb_control_transfer(libusb_device_handle *dev_handle,
	uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	unsigned char *data, uint16_t wLength, unsigned int timeout)
{
  debug_log("libusb_control_transfer(...)");

  // validate the device handle
  if(dev_handle != DEFAULT_LIBUSB_DEVICE) return LIBUSB_ERROR_INVALID_PARAM;

  // run the control transfer and return the number of bytes transferred
  return control_transfer(request_type, bRequest, wValue, wIndex, data, wLength, timeout);
}


void libusb_free_transfer(struct libusb_transfer *transfer)
{
  debug_log("libusb_free_transfer(...)");
  free(transfer);
}


libusb_device_handle * libusb_open_device_with_vid_pid( libusb_context *ctx, uint16_t vendor_id, uint16_t product_id)
{
  debug_log("libusb_open_device_with_vid_pid(...)");

  // validate the context
  if(ctx != DEFAULT_LIBUSB_CONTEXT) return LIBUSB_ERROR_INVALID_PARAM;

  // attempt to open the device
  if(open_device_with_vid_pid(vendor_id, product_id) < 0) {
    return LIBUSB_ERROR_NO_DEVICE;
  }

  return DEFAULT_LIBUSB_DEVICE;
}


int libusb_submit_transfer(struct libusb_transfer *transfer)
{
  debug_log("libusb_submit_transfer(...)");

  bool dir_in = (transfer->endpoint & 0x80) == 0x80;
  uint8_t ep = transfer->endpoint & 0x7f;

  add_pending_transfer(transfer);

  transfer->status = -1;

  switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_BULK:
      if(dir_in) return submit_bulk_in_transfer(ep, 
                                                transfer->length, 
                                                transfer->buffer, 
                                                transfer);
      else return submit_bulk_out_transfer(ep, 
                                           transfer->length, 
                                           transfer->buffer, 
                                           transfer);
      break;
    default:
      printf("Transfer type not implemented: %u", transfer->type);
      return 0;
  }

  return LIBUSB_SUCCESS;
}


int libusb_handle_events_timeout(libusb_context *ctx, struct timeval *tv)
{
  // debug_log("libusb_handle_events_timeout(...)"); 
  process_completed_transfers();
}

int libusb_cancel_transfer(struct libusb_transfer *transfer)
{
  debug_log("libusb_cancel_transfer(...)"); 
  transfer->status = LIBUSB_TRANSFER_CANCELLED;
}


/******************************************
 * HERE BE DRAGONS AND UNDEFINED BEHAVIOR *
 ******************************************/ 

int libusb_bulk_transfer(libusb_device_handle *dev_handle,
	unsigned char endpoint, unsigned char *data, int length,
	int *actual_length, unsigned int timeout)
{
  fprintf(stderr, "not implemented: libusb_bulk_transfer\n");
}

int libusb_interrupt_transfer(libusb_device_handle *dev_handle,
	unsigned char endpoint, unsigned char *data, int length,
	int *actual_length, unsigned int timeout)
{
  fprintf(stderr, "not implemented: libusb_interrupt_transfer\n");  
}

void libusb_transfer_set_stream_id(struct libusb_transfer *transfer, uint32_t stream_id)
{
  fprintf(stderr, "not implemented: libusb_transfer_set_stream_id\n");
}

uint32_t libusb_transfer_get_stream_id(struct libusb_transfer *transfer)
{
  fprintf(stderr, "not implemented: libusb_transfer_get_stream_id\n");
}

void libusb_set_debug(libusb_context *ctx, int level)
{
  fprintf(stderr, "not implemented: libusb_set_debug\n");
}

void libusb_set_log_cb(libusb_context *ctx, libusb_log_cb cb, int mode)
{
  fprintf(stderr, "not implemented: libusb_set_log_cb\n");
}

const struct libusb_version * libusb_get_version(void)
{
  fprintf(stderr, "not implemented: libusb_get_version\n");
}

int libusb_has_capability(uint32_t capability)
{
  fprintf(stderr, "not implemented: libusb_has_capability\n");
}

const char * libusb_error_name(int errcode)
{
  fprintf(stderr, "not implemented: libusb_error_name\n");
}

int libusb_setlocale(const char *locale)
{
  fprintf(stderr, "not implemented: libusb_setlocale\n");
}

const char * libusb_strerror(int errcode)
{
  fprintf(stderr, "not implemented: libusb_strerror\n");
}

libusb_device * libusb_ref_device(libusb_device *dev)
{
  fprintf(stderr, "not implemented: libusb_ref_device\n");
}

void libusb_unref_device(libusb_device *dev)
{
  fprintf(stderr, "not implemented: libusb_unref_device\n");
}

int libusb_get_config_descriptor(libusb_device *dev, uint8_t config_index, struct libusb_config_descriptor **config)
{
  fprintf(stderr, "not implemented: libusb_get_config_descriptor\n");
}

int libusb_get_config_descriptor_by_value(libusb_device *dev, uint8_t bConfigurationValue, struct libusb_config_descriptor **config)
{
  fprintf(stderr, "not implemented: libusb_get_config_descriptor_by_value\n");
}

int libusb_get_ss_endpoint_companion_descriptor( libusb_context *ctx, const struct libusb_endpoint_descriptor *endpoint, struct libusb_ss_endpoint_companion_descriptor **ep_comp)
{
  fprintf(stderr, "not implemented: libusb_get_ss_endpoint_companion_descriptor\n");
}

void libusb_free_ss_endpoint_companion_descriptor( struct libusb_ss_endpoint_companion_descriptor *ep_comp)
{
  fprintf(stderr, "not implemented: libusb_free_ss_endpoint_companion_descriptor\n");
}

int libusb_get_bos_descriptor(libusb_device_handle *dev_handle, struct libusb_bos_descriptor **bos)
{
  fprintf(stderr, "not implemented: libusb_get_bos_descriptor\n");
}

void libusb_free_bos_descriptor(struct libusb_bos_descriptor *bos)
{
  fprintf(stderr, "not implemented: libusb_free_bos_descriptor\n");
}

int libusb_get_usb_2_0_extension_descriptor( libusb_context *ctx, struct libusb_bos_dev_capability_descriptor *dev_cap, struct libusb_usb_2_0_extension_descriptor **usb_2_0_extension)
{
  fprintf(stderr, "not implemented: libusb_get_usb_2_0_extension_descriptor\n");
}

void libusb_free_usb_2_0_extension_descriptor( struct libusb_usb_2_0_extension_descriptor *usb_2_0_extension)
{
  fprintf(stderr, "not implemented: libusb_free_usb_2_0_extension_descriptor\n");
}

int libusb_get_ss_usb_device_capability_descriptor( libusb_context *ctx, struct libusb_bos_dev_capability_descriptor *dev_cap, struct libusb_ss_usb_device_capability_descriptor **ss_usb_device_cap)
{
  fprintf(stderr, "not implemented: libusb_get_ss_usb_device_capability_descriptor\n");
}

void libusb_free_ss_usb_device_capability_descriptor( struct libusb_ss_usb_device_capability_descriptor *ss_usb_device_cap)
{
  fprintf(stderr, "not implemented: libusb_free_ss_usb_device_capability_descriptor\n");
}

int libusb_get_container_id_descriptor(libusb_context *ctx, struct libusb_bos_dev_capability_descriptor *dev_cap, struct libusb_container_id_descriptor **container_id)
{
  fprintf(stderr, "not implemented: libusb_get_container_id_descriptor\n");
}

void libusb_free_container_id_descriptor( struct libusb_container_id_descriptor *container_id)
{
  fprintf(stderr, "not implemented: libusb_free_container_id_descriptor\n");
}

uint8_t libusb_get_port_number(libusb_device *dev)
{
  fprintf(stderr, "not implemented: libusb_get_port_number\n");
}

int libusb_get_port_path(libusb_context *ctx, libusb_device *dev, uint8_t *path, uint8_t path_length)
{
  fprintf(stderr, "not implemented: libusb_get_port_path\n");
}

libusb_device * libusb_get_parent(libusb_device *dev)
{
  fprintf(stderr, "not implemented: libusb_get_parent\n");
}

int libusb_get_device_speed(libusb_device *dev)
{
  fprintf(stderr, "not implemented: libusb_get_device_speed\n");
}

int libusb_get_max_packet_size(libusb_device *dev, unsigned char endpoint)
{
  fprintf(stderr, "not implemented: libusb_get_max_packet_size\n");
}

int libusb_get_max_iso_packet_size(libusb_device *dev, unsigned char endpoint)
{
  fprintf(stderr, "not implemented: libusb_get_max_iso_packet_size\n");
}

int libusb_wrap_sys_device(libusb_context *ctx, intptr_t sys_dev, libusb_device_handle **dev_handle)
{
  fprintf(stderr, "not implemented: libusb_wrap_sys_device\n");
}

int libusb_set_configuration(libusb_device_handle *dev_handle, int configuration)
{
  fprintf(stderr, "not implemented: libusb_set_configuration\n");
}

int libusb_set_interface_alt_setting(libusb_device_handle *dev_handle, int interface_number, int alternate_setting)
{
  fprintf(stderr, "not implemented: libusb_set_interface_alt_setting\n");
}

int libusb_clear_halt(libusb_device_handle *dev_handle, unsigned char endpoint)
{
  fprintf(stderr, "not implemented: libusb_clear_halt\n");
}

int libusb_reset_device(libusb_device_handle *dev_handle)
{
  fprintf(stderr, "not implemented: libusb_reset_device\n");
}

int libusb_alloc_streams(libusb_device_handle *dev_handle, uint32_t num_streams, unsigned char *endpoints, int num_endpoints)
{
  fprintf(stderr, "not implemented: libusb_alloc_streams\n");
}

int libusb_free_streams(libusb_device_handle *dev_handle, unsigned char *endpoints, int num_endpoints)
{
  fprintf(stderr, "not implemented: libusb_free_streams\n");
}

unsigned char * libusb_dev_mem_alloc(libusb_device_handle *dev_handle, size_t length)
{
  fprintf(stderr, "not implemented: libusb_dev_mem_alloc\n");
}

int libusb_dev_mem_free(libusb_device_handle *dev_handle, unsigned char *buffer, size_t length)
{
  fprintf(stderr, "not implemented: libusb_dev_mem_free\n");
}

int libusb_detach_kernel_driver(libusb_device_handle *dev_handle, int interface_number)
{
  fprintf(stderr, "not implemented: libusb_detach_kernel_driver\n");
}

int libusb_attach_kernel_driver(libusb_device_handle *dev_handle, int interface_number)
{
  fprintf(stderr, "not implemented: libusb_attach_kernel_driver\n");
}

int libusb_set_auto_detach_kernel_driver( libusb_device_handle *dev_handle, int enable)
{
  fprintf(stderr, "not implemented: libusb_set_auto_detach_kernel_driver\n");
}

int libusb_try_lock_events(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: libusb_try_lock_events\n");
}

void libusb_lock_events(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: libusb_lock_events\n");
}

void libusb_unlock_events(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: libusb_unlock_events\n");
}

int libusb_event_handling_ok(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: libusb_event_handling_ok\n");
}

int libusb_event_handler_active(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: implemented\n");
}

void libusb_interrupt_event_handler(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: libusb_interrupt_event_handler\n");
}

void libusb_lock_event_waiters(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: libusb_lock_event_waiters\n");
}

void libusb_unlock_event_waiters(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: libusb_unlock_event_waiters\n");
}

int libusb_wait_for_event(libusb_context *ctx, struct timeval *tv)
{
  fprintf(stderr, "not implemented: libusb_wait_for_event\n");
}

int libusb_handle_events_timeout_completed(libusb_context *ctx,
	struct timeval *tv, int *completed)
{
  fprintf(stderr, "not implemented: libusb_handle_events_timeout_completed\n");
}

int libusb_handle_events(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: libusb_handle_events\n");
}

int libusb_handle_events_completed(libusb_context *ctx, int *completed)
{
  fprintf(stderr, "not implemented: libusb_handle_events_completed\n");
}

int libusb_handle_events_locked(libusb_context *ctx,
	struct timeval *tv)
{
  fprintf(stderr, "not implemented: libusb_handle_events_locked\n");
}

int libusb_pollfds_handle_timeouts(libusb_context *ctx)
{
  fprintf(stderr, "not implemented: libusb_pollfds_handle_timeouts\n");
}

int libusb_get_next_timeout(libusb_context *ctx,
	struct timeval *tv)
{
  fprintf(stderr, "not implemented: libusb_get_next_timeout\n");
}