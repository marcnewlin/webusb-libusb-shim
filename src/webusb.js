
const DESCRIPTOR_INDEX_MANUFACTURER = 1;
const DESCRIPTOR_INDEX_PRODUCT = 2;
const DESCRIPTOR_INDEX_SERIAL_NUMBER = 3;
const DESCRIPTOR_INDEX_CONFIG = 4;
const DESCRIPTOR_INDEX_INTERFACE = 5;


// active USB device
var active_device = undefined;


// set the active USB device
function _set_active_device(device) {
  active_device = device;
}


// helper functions to retrieve the current VID/PID
// - to be run on the main thread context
function _get_vid() { return runtime_config.usb.vid; }
function _get_pid() { return runtime_config.usb.pid; }


// open the active device
function _open_device() {
  return Asyncify.handleAsync(async () => {
    await active_device.open();
  });
}


// open a device by vid/pid
function _open_device_by_vid_pid(vid, pid) {
  return Asyncify.handleAsync(async () => {
    if(await _request_usb_device_async(vid, pid) == 0) {
      return -1;
    }
    await active_device.open();
    return 1;
  });
}


// close the active device
function _close_device() {
  return Asyncify.handleAsync(async () => {
    return await active_device.close();
  });
}


// claim an interface
function _claim_interface(interface_number) {
  return Asyncify.handleAsync(async () => {
    return await active_device.claimInterface(interface_number);
  });
}


// release an interface
function _release_interface(interface_number) {
  return Asyncify.handleAsync(async () => {
    return await active_device.releaseInterface(interface_number);
  });
}


// get the current configuration value
function _get_configuration() {
  return active_device.configuration.configurationValue;
}


// find and authorize a USB device
function _request_usb_device() {
  return Asyncify.handleAsync(async () => {
    return await _request_usb_device_async(runtime_config.usb.vid, runtime_config.usb.pid);
  });
}


// find and authorize a USB device - async
async function  _request_usb_device_async(vendor_id, product_id) {
  if(vendor_id === undefined) vendor_id = runtime_config.usb.vid;
  if(product_id === undefined) vendor_id = runtime_config.usb.pid;

  // get the list of authorized devices
  let devices = await navigator.usb.getDevices();
  console.log(devices);

  // filter for the specified VID/PID
  let filtered = devices.filter(function(d) {
    return d.vendorId == vendor_id && d.productId == product_id;
  });

  // if we found one or more matching devices, 
  // set the first one as the active device
  if(filtered.length > 0) {
    _set_active_device(filtered[0]);
    console.debug(`The requested USB device ${vendor_id.toString(16)}:${product_id.toString(16)} was found.`);
    return 1;
  }

  // request access if we didn't find a matching device
  let f = { vendorId: vendor_id, productId: product_id };
  let device = await navigator.usb.requestDevice({filters: [f]});

  // if we were granted access to a device, 
  // set it as the active device
  if(device !== undefined) {
    _set_active_device(device);
    console.debug(`The requested USB device ${vendor_id.toString(16)}:${product_id.toString(16)} was found.`);
    return 1;
  }

  // handle failures
  console.error("Failed to find/authorize USB device.");
  return 0;
}


// generate the device descriptor
function _get_device_descriptor(desc) {

  const DEVICE_DESCRIPTOR_LENGTH = 18;
  const LIBUSB_DT_DEVICE = 1;

  let d = active_device;

  // serialize a libusb_device_descriptor blob
  let data = new Uint8Array(DEVICE_DESCRIPTOR_LENGTH);
  data[0] = DEVICE_DESCRIPTOR_LENGTH;
  data[1] = LIBUSB_DT_DEVICE
  data[2] = d.usbVersionMajor,
  data[3] = d.usbVersionMinor;
  data[4] = d.deviceClass;
  data[5] = d.deviceSubClass;
  data[6] = d.deviceProtocol;
  data[7] = 64; // TODO figure out where to get this value from
  data[8] = d.vendorId & 0xff;
  data[9] = d.vendorId >> 8;
  data[10] = d.productId & 0xff;
  data[11] = d.productId >> 8;
  data[12] = d.deviceVersionMajor;
  data[13] = (d.deviceVersionMinor << 4) | d.deviceVersionSubminor;
  data[14] = DESCRIPTOR_INDEX_MANUFACTURER;
  data[15] = DESCRIPTOR_INDEX_PRODUCT;
  data[16] = DESCRIPTOR_INDEX_SERIAL_NUMBER;
  data[17] = d.configurations.length;

  // write the descriptor to the heap
  writeArrayToMemory(data, desc);
}


// get a string descriptor by index
function _get_string_descriptor(index, buffer, length) {
  
  // lookup the descriptor 
  let desc = null;
  switch(index) {
    case DESCRIPTOR_INDEX_SERIAL_NUMBER:
      desc = active_device.serialNumber;
      break;
    case DESCRIPTOR_INDEX_PRODUCT:
      desc = active_device.productName;
      break;
    case DESCRIPTOR_INDEX_MANUFACTURER:
      desc = active_device.manufacturerName;
      break;
    default:
      throw `Unhandled string descriptor, index ${index}`;
  }

  // copy the descriptor to the heap
  let len = Math.min(lengthBytesUTF8(desc)+1, length);
  stringToUTF8(desc, buffer, len);

  // return the length
  return len;
}


// get the active configuration descriptor
function _get_active_config_descriptor() {

  const CONFIG_DESCRIPTOR_LENGTH = 9;
  const INTERFACE_DESCRIPTOR_LENGTH = 9;
  const ENDPOINT_DESCRIPTOR_LENGTH = 7;
  const LIBUSB_DT_CONFIG = 2;
  const LIBUSB_DT_INTERFACE = 4;
  const LIBUSB_DT_ENDPOINT = 5;

  let device = active_device;

  // compute the number of endpoints in the current configuration
  let num_interfaces = 0;
  let num_endpoints = 0;
  for(let i of device.configuration.interfaces) {
    num_interfaces += i.alternates.length;
    for(let alt of i.alternates) {
      num_endpoints += alt.endpoints.length;
    }
  }

  // allocate the buffer
  let descriptor_length = num_endpoints * ENDPOINT_DESCRIPTOR_LENGTH + 
                          num_interfaces * INTERFACE_DESCRIPTOR_LENGTH + 
                          CONFIG_DESCRIPTOR_LENGTH;
  let data = new Uint8Array(descriptor_length);

  // libusb_config_descriptor
  data[0] = CONFIG_DESCRIPTOR_LENGTH;
  data[1] = LIBUSB_DT_CONFIG;
  data[2] = descriptor_length & 0xff;
  data[3] = descriptor_length >> 8;
  data[4] = num_interfaces;
  data[5] = device.configuration.configurationValue;
  data[6] = DESCRIPTOR_INDEX_CONFIG;
  data[7] = 0; // bmAttributes
  data[8] = 0; // MaxPower

  // libusb_interface_descriptors
  let offset = 9;
  for(let i of device.configuration.interfaces) {
    for(let alt of i.alternates) {
      data[offset+0] = INTERFACE_DESCRIPTOR_LENGTH;
      data[offset+1] = LIBUSB_DT_INTERFACE;
      data[offset+2] = alt.interfaceNumber;
      data[offset+3] = alt.alternateSetting;
      data[offset+4] = alt.endpoints.length;
      data[offset+5] = alt.interfaceClass;
      data[offset+6] = alt.interfaceSubClass;
      data[offset+7] = alt.interfaceProtocol;
      data[offset+8] = DESCRIPTOR_INDEX_INTERFACE;
      offset += INTERFACE_DESCRIPTOR_LENGTH;

      // libusb_endpoint_descriptors
      for(let ep of alt.endpoints) {
        let ep_addr = ep.endpointNumber;
        if(ep.direction == "in") ep_addr |= 0x80;
        data[offset+0] = ENDPOINT_DESCRIPTOR_LENGTH;
        data[offset+1] = LIBUSB_DT_ENDPOINT;
        data[offset+2] = ep_addr;
        data[offset+3] = 0; // bmAttributes
        data[offset+4] = ep.packetSize & 0xff;
        data[offset+5] = ep.packetSize >> 8;
        data[offset+6] = 0; // bInterval
        offset += ENDPOINT_DESCRIPTOR_LENGTH;
      }
    }
  }

  // write it to the wasm heap
  let buffer = _malloc(data.length);
  writeArrayToMemory(data, buffer);
  
  // return the heap pointer
  return buffer;
}


// run a control transfer
function _control_transfer(bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout) { 
  
  if(timeout != 0) {
    throw "TODO implement control transfer timeout handling";
  }

  return Asyncify.handleAsync(async () => {

    // transfer option lookup tables
    let libusb_request_type = ["standard", "class", "vendor"];
    let libusb_request_recipient = ["device", "interface", "endpoint", "other"];

    // parse the request type
    let req_type = libusb_request_type[(bmRequestType & 0x60) >> 5];

    // parse the recipient type
    let rec_type = libusb_request_recipient[(bmRequestType & 0x1f)];

    // parse the request direction
    let dir_in = (bmRequestType & 0x80) == 0x80;

    // configure the transfer options
    let setup = {
      requestType: req_type,
      recipient: rec_type,
      request: bRequest,
      value: wValue,
      index: wIndex,
    };

    // input transfer
    if(dir_in) {

      // perform the transfer
      let result = await active_device.controlTransferIn(setup, wLength);
      if(result.status != "ok") {
        throw `controlTransferIn response: ${result}`;
      }

      // copy the data to the buffer on the heap
      let view = new Uint8Array(result.data.buffer);
      writeArrayToMemory(view, data);

      // return the length of data read
      return result.data.buffer.byteLength;
    }

    // output transfer
    else {

      // fill the output buffer
      let buffer = new Uint8Array(wLength);
      for(let x = 0; x < wLength; x++) {
        buffer[x] = getValue(data+x, "i8");
      }

      // perform the transfer
      let result = await active_device.controlTransferOut(setup, buffer);
      if(result.status != "ok") {
        throw `controlTransferOut response: ${result}`;
      }

      // return the length of data written
      return result.bytesWritten;
    }
  });
}


// submit an asynchronous bulk input transfer
async function _submit_bulk_in_transfer(ep, len, buffer, transfer) {
       
  const LIBUSB_TRANSFER_COMPLETED = 0;
  const LIBUSB_TRANSFER_CANCELLED = 3;
  const LIBUSB_SUCCESS = 0;

  if(active_device === undefined) {
    console.warn("_submit_bulk_in_transfer called when active_device === undefined");
    return false;
  }

  // perform the transfer
  let result;
  try {
    result = await active_device.transferIn(ep, len);
  } catch (error) {
    console.warn("transfer error in _submit_bulk_in_transfer");
    return false;
  } 

  // write the received data to the heap buffer
  let data = new Uint8Array(result.data.buffer);
  writeArrayToMemory(data, buffer);

  // set the transfer status to completed
  setValue(transfer+20, data.length, "i32");                      // actualLength
  if(getValue(transfer+12, "i32") != LIBUSB_TRANSFER_CANCELLED) { // status
    setValue(transfer+12, LIBUSB_TRANSFER_COMPLETED, "i32");      // status
  }

  return LIBUSB_SUCCESS;
}


// submit an asynchronous bulk output transfer
async function _submit_bulk_out_transfer(ep, len, buffer, transfer) {

  const LIBUSB_TRANSFER_COMPLETED = 0;
  const LIBUSB_TRANSFER_CANCELLED = 3;
  const LIBUSB_SUCCESS = 0;

  if(active_device === undefined) {
    console.warn("_submit_bulk_in_transfer called when active_device === undefined");
    return false;
  }

  // perform the transfer
  let result;
  let data;
  try {
    data = new Uint8Array(len);
    for(let x = 0; x < len; x++) {
      data[x] = getValue(buffer+x, "i8");
    }
    result = await active_device.transferOut(ep, data);
  } catch (error) {
    console.warn("transfer error in _submit_bulk_out_transfer");
    return false;
  } 

  // set the transfer status to completed
  setValue(transfer+20, result.bytesWritten, "i32");                      // actualLength
  if(getValue(transfer+12, "i32") != LIBUSB_TRANSFER_CANCELLED) { // status
    setValue(transfer+12, LIBUSB_TRANSFER_COMPLETED, "i32");      // status
  }

  return LIBUSB_SUCCESS;
}
