
var Module = {
  noInitialRun: true,
  onRuntimeInitialized: runtime_initialized,
  print: handle_stdout,
  printErr: handle_stderr,
};


// add a line to the terminal history div
function add_terminal_line(msg, type) {
  let line = document.createElement("div");
  line.innerHTML = msg;
  line.setAttribute("class", `terminal-line ${type}`);
  let th = document.getElementById("terminal-history");
  th.appendChild(line);
}


// handle stdout messages from wasm
function handle_stdout(msg) {
  add_terminal_line(msg, "stdout");
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


// run a wasm loader by adding it as a script tag
function run_wasm_loader(path) {
  let script = document.createElement("script");
  script.setAttribute("src", path);
  document.body.appendChild(script);
}


// run our initialization code after the page has been rendered
document.addEventListener("DOMContentLoaded", () => {
  run_wasm_loader(runtime_config.app.loader);
});


// called when the Emscripten runtime environment is ready
function runtime_initialized() {

  // add an output line showing the invocation
  handle_stdout(`$ ${runtime_config.app.loader.split(".")[0]} ${runtime_config.app.args.join(" ")}`);

  // add a "Go" button
  let line = document.getElementsByClassName("stdout")[0];
  let run = document.createElement("button");
  run.innerHTML = "Start";
  run.setAttribute("id", "go");
  run.onclick = function() {

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
    
    // call main(...)
    Module.ccall("main", "number", ["number", "number"], [args.length, argv], { async: true }).then(() => {
      if(runtime_config.app.out_file !== undefined) {
        emit_memfs_file(runtime_config.app.out_file);
      }
    });

    // hide the button
    run.style = "display: none";
  };
  line.appendChild(run);
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