const TrieWasm = (() => {
  let instance = null;

  function getMemory() {
    return new Uint8Array(instance.exports.memory.buffer);
  }

  function writeString(str) {
    const ptr = instance.exports.alloc(str.length + 1);
    const mem = getMemory();
    for (let i = 0; i < str.length; i++) {
      mem[ptr + i] = str.charCodeAt(i);
    }
    mem[ptr + str.length] = 0;
    return ptr;
  }

  function readString(ptr) {
    const mem = getMemory();
    let s = "";
    for (let i = ptr; mem[i] !== 0; i++) {
      s += String.fromCharCode(mem[i]);
    }
    return s;
  }

  async function load(wasmPath) {
    const response = await fetch(wasmPath);
    const bytes = await response.arrayBuffer();
    const imports = {
      env: {
        js_log: (ptr, len) => {
          const mem = new Uint8Array(instance.exports.memory.buffer, ptr, len);
          console.log(new TextDecoder().decode(mem));
        },
      },
    };
    const result = await WebAssembly.instantiate(bytes, imports);
    instance = result.instance;
    instance.exports.trie_init();
  }

  function init() {
    instance.exports.trie_init();
  }

  function insert(cidr) {
    const ptr1 = writeString(cidr);
    const r1 = instance.exports.trie_insert(ptr1);
    const ptr2 = writeString(cidr);
    instance.exports.ctrie_insert(ptr2);
    const ptr3 = writeString(cidr);
    instance.exports.ptrie_insert(ptr3);
    return r1;
  }

  function remove(cidr) {
    const ptr1 = writeString(cidr);
    const r1 = instance.exports.trie_delete(ptr1);
    const ptr2 = writeString(cidr);
    instance.exports.ctrie_delete(ptr2);
    const ptr3 = writeString(cidr);
    instance.exports.ptrie_delete(ptr3);
    return r1;
  }

  function lookup(ip, viewMode) {
    const fn = viewMode === "compressed" ? "ctrie_lookup"
             : viewMode === "patricia"  ? "ptrie_lookup"
             : "trie_lookup";
    const ptr = writeString(ip);
    const resultPtr = instance.exports[fn](ptr);
    return readString(resultPtr);
  }

  function serialize() {
    const resultPtr = instance.exports.trie_serialize();
    const json = readString(resultPtr);
    return json ? JSON.parse(json) : null;
  }

  function serializeCompressed() {
    const resultPtr = instance.exports.ctrie_serialize();
    const json = readString(resultPtr);
    return json ? JSON.parse(json) : null;
  }

  function serializePatricia() {
    const resultPtr = instance.exports.ptrie_serialize();
    const json = readString(resultPtr);
    return json ? JSON.parse(json) : null;
  }

  function stats(viewMode) {
    const fns = {
      standard: { count: "trie_node_count", size: "trie_node_size" },
      compressed: { count: "ctrie_node_count", size: "ctrie_node_size" },
      patricia: { count: "ptrie_node_count", size: "ptrie_node_size" },
    };
    const f = fns[viewMode] || fns.standard;
    const count = instance.exports[f.count]();
    const size = instance.exports[f.size]();
    return { nodeCount: count, nodeSize: size, totalMemory: count * size };
  }

  return { load, init, insert, remove, lookup, serialize, serializeCompressed, serializePatricia, stats };
})();
