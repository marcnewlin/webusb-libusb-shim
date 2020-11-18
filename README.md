# webusb-libusb-shim

Experimental libusb-WebUSB shim to enable compiling 
libusb-based C-programs to Wasm, and have the USB 
logic "just work".

This is *experimental and incomplete*, but is sufficiently functional to build unmodified `libhackrf` and `hackrf_transfer` to Wasm, run it in a browser,
receive IQ to `MEMFS`, and download the resulting IQ file.

![](demo.gif)

## build and run

```
$ git clone https://github.com/marcnewlin/webusb-libusb-shim.git

$ cd webusb-libusb-shim

$ git submodule init && git submodule update

$ make

$ cd build

$ python3 -m http.server
```

Navigate to [http://127.0.0.1:8000/](http://127.0.0.1:8000/) in Chrome (or another compatible browser), and press `Start`.

