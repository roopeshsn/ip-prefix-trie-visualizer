# IP Prefix Trie Visualizer

## Architecture
## How It Works

### C++ → WASM (no Emscripten)

The C++ code compiles to WASM using `clang --target=wasm32-unknown-unknown`. There is no libc — the project implements its own:

- **Bump allocator** — 1 MB static heap with aligned allocation
- **String utilities** — minimal `strlen`, `strcpy` for the WASM boundary
- **IP/CIDR parser** — parses `A.B.C.D/N` notation in C++
- **JSON serializer** — hand-written JSON output for D3 consumption
- **Console logging** — `js_log()` imported from JS, called from C++ to log to the browser console

### WASM ↔ JS Boundary

The JS glue (`wasm-glue.js`) handles:

- Loading and instantiating the `.wasm` file via `WebAssembly.instantiate()`
- Writing strings into WASM linear memory (`writeString`)
- Reading null-terminated strings back (`readString`)
- Providing the `js_log` import for C++ → console.log bridge
- Wrapping exported functions into a clean API (`TrieWasm.insert()`, `.lookup()`, etc.)

### Visualization

- **Standard/Compressed** — `d3.hierarchy()` + `d3.tree()` layout, SVG rendering
- **PATRICIA** — graph-based layout (nodes + edges JSON), forward edges as solid lines, back-pointers as dashed red arcs with arrowheads

## Make Targets

| Target | Description |
|--------|-------------|
| `make` | Build `build/trie.wasm` |
| `make serve` | Build and start a dev server on port 8080 |
| `make clean` | Remove the `build/` directory |
| `make setup` | Install LLVM via Homebrew |