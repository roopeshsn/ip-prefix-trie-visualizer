# ---- Toolchain paths (brew install llvm) ----
LLVM_PREFIX := $(shell brew --prefix llvm 2>/dev/null || echo /opt/homebrew/opt/llvm)
CLANG       := $(LLVM_PREFIX)/bin/clang++

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
	@echo "Installing LLVM via Homebrew (provides clang with wasm32 support)..."
	brew install llvm
	@echo ""
	@echo "Done. Make sure $(LLVM_PREFIX)/bin is accessible."
	@echo "Run 'make' to build the WASM module."
