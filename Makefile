# given a space-delimited list of strings, generate a double-quote-enclosed CSV-string
comma=,
list-to-csv=$(subst $() $(),$(comma),$(strip $(addsuffix ",$(addprefix ",${1}))))


# libusb shim - C source files
LIBUSB_SOURCE=src/libusb.c \
							src/webusb.c


# functions emcc should ignore when pruning unused functions
LIBUSB_EXPORTS=_main \
							 _libusb_exit \
							 _malloc


# Emscripten runtime functions exported to JS
RUNTIME_EXPORTS=callMain \
								ccall \
								FS \
								lengthBytesUTF8 \
								stringToUTF8 \
								setValue \
								addOnExit


# async JS functions usable by Asyncify
ASYNCIFY_FUNCS=request_device_access \
							 open_device \
							 open_device_with_vid_pid \
							 close_device \
							 claim_interface \
							 release_interface \
							 control_transfer \
							 emscripten_receive_on_main_thread_js \
							 emscripten_asm_const_iii


# include directories
INCLUDE=-I/usr/include/libusb-1.0


# Emscripten flags
FLAGS=-s WASM=1 \
			-s EXPORTED_FUNCTIONS=[$(call list-to-csv, $(LIBUSB_EXPORTS))] \
			-s ASYNCIFY_IMPORTS=[$(call list-to-csv, $(ASYNCIFY_FUNCS))] \
			-s EXTRA_EXPORTED_RUNTIME_METHODS=[$(call list-to-csv, $(RUNTIME_EXPORTS))] \
			-s ASYNCIFY=1 \
			-s SAFE_HEAP=0 \
			-s EXIT_RUNTIME=1 \
			-s PROXY_TO_PTHREAD=1 \
			-s FORCE_FILESYSTEM=1 \
			--pre-js src/webusb.js \
			-pthread


.PHONY: client

all: hackrf_info hackrf_clock hackrf_transfer hackrf_spiflash

clean:
	rm -rf build/
	mkdir -p build

hackrf_%: client
	emcc $(FLAGS) $(INCLUDE) -o build/$@.js $(LIBUSB_SOURCE) -DTOOL_RELEASE='"wasm"' -Iexternal/hackrf/host/libhackrf/src external/hackrf/host/libhackrf/src/hackrf.c external/hackrf/host/hackrf-tools/src/$@.c

client:
	cp client/* build/
	cp -r assets build/
