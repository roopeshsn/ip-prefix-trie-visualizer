# ---- Toolchain detection (macOS via Homebrew, Linux via system packages) ----
LLVM_PREFIX := $(shell brew --prefix llvm 2>/dev/null)
ifdef LLVM_PREFIX
  CLANG := $(LLVM_PREFIX)/bin/clang++
else
  CLANG := clang++
endif

SOURCES  := src/trie.cpp src/ctrie.cpp src/ptrie.cpp
CXXFLAGS := --target=wasm32-unknown-unknown -O2 -nostdlib -ffreestanding -fno-exceptions -fno-rtti
LDFLAGS  := -Wl,--no-entry \
            -Wl,--export=alloc \
            -Wl,--export=trie_init \
            -Wl,--export=trie_insert \
            -Wl,--export=trie_delete \
            -Wl,--export=trie_lookup \
            -Wl,--export=trie_serialize \
            -Wl,--export=trie_node_count \
            -Wl,--export=trie_node_size \
            -Wl,--export=ctrie_lookup \
            -Wl,--export=ctrie_insert \
            -Wl,--export=ctrie_delete \
            -Wl,--export=ctrie_serialize \
            -Wl,--export=ctrie_node_count \
            -Wl,--export=ctrie_node_size \
            -Wl,--export=ptrie_lookup \
            -Wl,--export=ptrie_insert \
            -Wl,--export=ptrie_delete \
            -Wl,--export=ptrie_serialize \
            -Wl,--export=ptrie_node_count \
            -Wl,--export=ptrie_node_size \
            -Wl,--allow-undefined \
            -Wl,--initial-memory=2097152

.PHONY: all clean serve setup

all: build/trie.wasm

build/trie.wasm: $(SOURCES) src/common.h | build
	$(CLANG) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SOURCES)

build:
	mkdir -p build

clean:
	rm -rf build

serve: all
	@echo "Serving at http://localhost:8080"
	python3 -m http.server 8080

setup:
	@if command -v brew >/dev/null 2>&1; then \
		echo "Installing LLVM via Homebrew..."; \
		brew install llvm; \
	elif command -v apt-get >/dev/null 2>&1; then \
		echo "Installing clang and lld via apt..."; \
		sudo apt-get update && sudo apt-get install -y clang lld; \
	else \
		echo "Unsupported package manager. Install clang and lld with wasm32 support manually."; \
	fi
	@echo ""
	@echo "Run 'make' to build the WASM module."
