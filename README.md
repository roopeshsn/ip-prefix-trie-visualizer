# IP Prefix Trie Visualizer

Interactive visualizer for three IP prefix trie variants — Standard, Compressed, and PATRICIA — built with C++ (compiled to WebAssembly) and D3.js.

## Quick Start

```sh
brew install llvm    # prerequisite: LLVM with wasm32 support
make serve           # builds WASM and starts server at http://localhost:8080
```

## Usage

- **Insert/Delete** — enter a CIDR (e.g. `10.0.0.0/8`)
- **Lookup** — enter an IP to see the longest prefix match highlighted
- **Bulk Import** — paste CIDRs or raw `show ip bgp` output
- **View Mode** — toggle between Standard, Compressed, and PATRICIA
- **Console** — open DevTools to see C++ logs for insert/delete/lookup traversals

## Project Structure

```
src/           C++ trie implementations (trie.cpp, ctrie.cpp, ptrie.cpp, common.h)
js/            WASM glue, D3 visualizer, app logic
css/           Styling
index.html     Entry point
Makefile       Build: make, make serve, make clean, make setup
```

## How It Works

C++ compiles to WASM via `clang --target=wasm32-unknown-unknown` — no Emscripten. The project implements its own bump allocator, IP parser, JSON serializer, and a `js_log()` bridge for C++ to log directly to the browser console. The JS glue handles string marshalling across the WASM boundary.
