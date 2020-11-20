var device_configs = {

  // HackRF
  hackrf: {

    // USB device configuration
    usb: {
      vid: 0x1d50,
      pid: 0x6089,
    },

    // application configurations
    app_configs: {

      // hackrf_info
      hackrf_info: {
        loader: "hackrf_info.js",
        args: [],
      },

      // hackrf_clock -a
      hackrf_clock: {
        loader: "hackrf_clock.js",
        args: ["-a"],
      },

      // hackrf_transfer -r out.iq -f 915000000 -n 10000000 -s 1000000
      hackrf_transfer_receive: {
        loader: "hackrf_transfer.js",
        args: ["-r", "receive.iq", "-f", "915000000", "-n", "10000000", "-s", "1000000"],
        output_files: ["receive.iq"],
      },

      // hackrf_transfer -t /tmp/test.iq -f 915000000 -s 1000000
      hackrf_transfer_transmit: {
        loader: "hackrf_transfer.js",
        args: ["-t", "/data/transmit.iq", "-f", "915000000", "-n", "10000000", "-s", "1000000"],
        input_files: [
          {
            // remote url, passed into a fetch(...) request
            remote_url: "/assets/hackrf/hackrf_transfer/transmit.iq",

            // target file path (MEMFS filesystem)
            local_path: "/data/transmit.iq",
          }
        ],
      },

      // hackrf_spiflash -v -w /firmware/portapack-h1_h2-mayhem.bin"
      hackrf_firmware_mayhem: {
        loader: "hackrf_spiflash.js",
        args: ["-v", "-w", "/firmware/portapack-h1_h2-mayhem.bin"],
        input_files: [
          {
            // remote url, passed into a fetch(...) request
            remote_url: "/assets/hackrf/hackrf_spiflash/portapack-h1_h2-mayhem.bin",

            // target file path (MEMFS filesystem)
            local_path: "/firmware/portapack-h1_h2-mayhem.bin",
          }
        ],
      },
    }
  }
}
