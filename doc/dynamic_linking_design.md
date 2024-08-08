# Dynamic linking Design Document

The purpose of this document is to present a design and explain the design decisions for implementing [Dynamic Linking](https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md) in WAMR.

## Scratchpad

### APIs
 * module loading search paths:
   * existing API for setting the reader, e.g. `wasm_runtime_dl_set_module_reader` pros: flexibility, can load modules not only from file system, cons: difficult to use
   * specify search paths, e.g. `wasm_runtime_dl_set_search_path` pros: simple to use, cons: limited only to file system
 * do we need `wasm_runtime_dl_register_module` ? Maybe not

### Module resolution
 * lazy loading of a module when the symbol is being used
 * option to pre-load everything to avoid unexpected slow downs

### Placing memory segments
 * in the heap area; considerations:
   * `malloc` must be exported
   * `malloc` can not use data segments
   * runtime might need to load dependent modules if `malloc` refers to imported functions
   * `malloc` might not be (and likely won't be) defined in the main module
 * in the stack area; considerations:
   * when compiling the main module, the data segments of submodules must be taken into account
   * any changes of the data segments in submodules requires linking again the main module with the new stack size (this might not be a big deal as likely the stack size requirements of the submodule will change more often anyways)
 * update `__heap_base` before instantiating the module; considerations
   * `__heap_base` is a non-mutable global; linker seem to replace any access to that global with a `const.i32 XXX` instead of placing `global.get __heap_base`. So the value can't be modified


### Multi-threading
 * all modules must be instantiated for each thread (or lazy loaded)
 * As of now, wasi-libc doesn't provide .so files for wasi-threads target: https://github.com/WebAssembly/wasi-libc/blob/230d4be6c54bec93181050f9e25c87150506bdd0/Makefile#L788
 * llvm seems to add `__tls_base` global for each of the submodule

### AOT