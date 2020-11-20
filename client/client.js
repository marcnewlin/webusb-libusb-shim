var Module = {
  noInitialRun: true,
  onRuntimeInitialized: runtime_initialized,
  print: handle_stdout,
  printErr: handle_stderr,
};


var runtime_config = undefined;


// add a line to the terminal history div
function add_terminal_line(msg, type) {

  // create and append the output line
  let line = document.createElement("div");
  line.innerHTML = msg;
  line.setAttribute("class", `terminal-line ${type}`);
  let th = document.getElementById("terminal-history");
  th.appendChild(line);

  // auto-scroll
  document.body.scrollTop = document.body.scrollHeight;
}


// handle stdout messages from wasm
function handle_stdout(msg) {
  add_terminal_line(msg, "stdout");
}


// append an "info" line to the output div
function print_info(msg) {
  add_terminal_line(msg, "info");
  console.warn(msg);
}


// append an "error" line to the output div
function print_error(msg) {
  add_terminal_line(msg, "error");
  console.error(msg);
}


// handle stderr messages from wasm
function handle_stderr(msg) {
  if(msg.startsWith("program exited (with status: 0)")) {
    // nop
  } else if(msg.startsWith("Calling stub instead of")) {
    // nop
  } else if(msg.startsWith("Blocking on the main thread")) {
    // nop
  } else {
    add_terminal_line(msg, "stderr");
  }
}


// run the wasm loader for a specified runtime config
function run_wasm_loader(config) {
  runtime_config = config;
  let script = document.createElement("script");
  script.setAttribute("src", config.app.loader);
  document.body.appendChild(script);
}


// generate the app-config selection list
function generate_app_list() {

  // flattened config list
  let runtime_configs = [];

  // step through the device configurations
  for(d in device_configs) {
    let device = device_configs[d];

    // for each configured app, generate a flattened runtime config
    for(let a in device.app_configs) {
      let app = device.app_configs[a];
      let cmdline = app.loader.split(".")[0];
      if(app.args.length > 0) cmdline = `${cmdline} ${app.args.join(" ")}`;
      runtime_configs.push({
        usb: device.usb,
        app: app,
        cmdline: cmdline,
      })
    }
  }

  // add a list entry for each config
  let list = document.getElementById("command-list");
  for(let c of runtime_configs) {
    
    // build the list-entry anchor, which kicks 
    // off the wasm loader when clicked 
    let a = document.createElement("a");
    a.setAttribute("href", "#")
    a.setAttribute("class", "cmd-list-entry");
    a.innerHTML = c.cmdline;
    a.onclick = function(e) {
      list.classList.add("disabled");
      a.classList.add("selected");
      run_wasm_loader(c);
    };

    // build the list-entry div
    let entry = document.createElement("div");
    entry.setAttribute("class", "app-config");
    entry.appendChild(a);

    // add the config to the command list
    list.appendChild(entry);
  }

  // update the output div such that it's offset below the floating command list
  document.getElementById("terminal-history").style.marginTop = `calc(${list.offsetHeight}px + 0.75em)`;
}


// run our initialization code after the page has been rendered
document.addEventListener("DOMContentLoaded", () => {
  generate_app_list();
});


// called when the Emscripten runtime environment is ready
async function runtime_initialized() {

  // prepend the loader name as the first argument
  let args = runtime_config.app.args;
  args.unshift(runtime_config.app.loader);

  // write the arguments to the wasm heap
  let arg_ptrs = [];
  for(let arg of args) {
    let len = Module.lengthBytesUTF8(arg)+4+1;
    let ptr = Module._malloc(len);
    Module.stringToUTF8(arg, ptr, len);
    arg_ptrs.push(ptr);
  }

  // build the argument pointer array
  let argv = Module._malloc(arg_ptrs.length*4);
  for(let x = 0; x < arg_ptrs.length; x++) {
    Module.setValue(argv+x*4, arg_ptrs[x], "i32");
  }
  
  // fetch any defined input files into memfs
  for(let i in runtime_config.app.input_files) {
    let f = runtime_config.app.input_files[i];

    // fetch the remote file
    print_info(`fetching '${f.remote_url}'`);
    let res = await fetch(f.remote_url);
    if(res.ok !== true) {
      print_error(`error fetching file '${f.remote_url}', aborting`);
      return;
    }
    
    // get the file contents as an ArrayBuffer
    let buff = new Uint8Array(await res.arrayBuffer());

    // parse the file name and parent directory
    let offset = f.local_path.lastIndexOf("/");
    let filename = f.local_path.substr(offset);
    let parent = f.local_path.substr(0, offset);

    // create the lazy-loaded file
    let can_read = true;
    let can_write = false;
    console.log(f.local_path);
    FS.mkdir(parent);
    FS.writeFile(f.local_path, buff);
  }

  // output the select command line invocation
  print_info(`running '${runtime_config.cmdline}'`);

  // call main(...)
  Module.ccall("main", "number", ["number", "number"], [args.length, argv], { async: true }).then((status) => {

    // log the main() return code
    print_info(`application exited with status code ${status}`);

    // attempt to emit the configured output files (as individual file downloads)
    for(let f of runtime_config.app.output_files) {
      emit_memfs_file(f);
    }
  });
}


// helper function to read a file from the Emscripten 
// in-memory filesystem and emit it for download 
function emit_memfs_file(path) {
  let data = Module.FS.readFile(path);
  let url = URL.createObjectURL(new Blob([data], {type: "application/octet-stream"}));
  var a = document.createElement("a");
  a.style = "display: none";
  a.href = url;
  a.download = path;
  document.body.appendChild(a);
  a.click();
}