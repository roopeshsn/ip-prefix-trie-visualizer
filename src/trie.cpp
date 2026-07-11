#include "common.h"

// --- Memory allocator (bump allocator over static heap) ---

static unsigned char heap[1024 * 1024]; // 1 MB
static int heap_offset = 0;

extern "C" void* alloc(int size) {
    int aligned = (heap_offset + 7) & ~7;
    if (aligned + size > (int)sizeof(heap)) return nullptr;
    void* ptr = &heap[aligned];
    heap_offset = aligned + size;
    return ptr;
}

static void heap_reset() {
    heap_offset = 0;
}

// --- Trie data structure ---

struct TrieNode {
    TrieNode* children[2];
    bool has_prefix;
    u32 prefix_ip;
    int prefix_len;
};

static TrieNode* root = nullptr;

static TrieNode* new_node() {
    TrieNode* n = (TrieNode*)alloc(sizeof(TrieNode));
    if (!n) return nullptr;
    n->children[0] = nullptr;
    n->children[1] = nullptr;
    n->has_prefix = false;
    n->prefix_ip = 0;
    n->prefix_len = 0;
    return n;
}

// --- Trie operations ---

extern "C" void trie_init() {
    heap_reset();
    root = new_node();
    ctrie_init_internal();
    ptrie_init_internal();
}

extern "C" int trie_insert(const char* cidr) {
    u32 ip;
    int plen;
    if (!parse_cidr(cidr, &ip, &plen)) return -1;

    TrieNode* node = root;
    for (int i = 0; i < plen; i++) {
        int bit = get_bit(ip, i);
        if (!node->children[bit]) {
            node->children[bit] = new_node();
            if (!node->children[bit]) return -1;
        }
        node = node->children[bit];
    }
    node->has_prefix = true;
    node->prefix_ip = ip;
    node->prefix_len = plen;

    log_begin();
    log_append("[Standard Trie] INSERT ");
    log_append(cidr);
    log_flush();

    return 0;
}

static bool delete_recursive(TrieNode* node, u32 ip, int plen, int depth) {
    if (depth == plen) {
        if (!node->has_prefix) return false;
        node->has_prefix = false;
        return !node->children[0] && !node->children[1];
    }

    int bit = get_bit(ip, depth);
    if (!node->children[bit]) return false;

    bool child_empty = delete_recursive(node->children[bit], ip, plen, depth + 1);
    if (child_empty) {
        node->children[bit] = nullptr;
    }

    return !node->has_prefix && !node->children[0] && !node->children[1];
}

extern "C" int trie_delete(const char* cidr) {
    u32 ip;
    int plen;
    if (!parse_cidr(cidr, &ip, &plen)) return -1;
    if (!root) return -1;

    delete_recursive(root, ip, plen, 0);

    log_begin();
    log_append("[Standard Trie] DELETE ");
    log_append(cidr);
    log_flush();

    return 0;
}

// --- Lookup ---

static char output_buf[64 * 1024];

extern "C" const char* trie_lookup(const char* ip_str) {
    u32 ip;
    int consumed;
    if (!parse_ip(ip_str, &ip, &consumed)) {
        output_buf[0] = '\0';
        return output_buf;
    }
    if (ip_str[consumed] != '\0') {
        output_buf[0] = '\0';
        return output_buf;
    }

    log_begin();
    log_append("[Standard Trie] LOOKUP ");
    log_append(ip_str);
    log_flush();

    TrieNode* node = root;
    const char* best_match = nullptr;
    static char match_buf[64];

    log_begin();
    log_append("  path: root");

    if (node && node->has_prefix) {
        format_cidr(node->prefix_ip, node->prefix_len, match_buf);
        best_match = match_buf;
    }

    for (int i = 0; i < 32 && node; i++) {
        int bit = get_bit(ip, i);
        if (!node->children[bit]) {
            log_append(" -> (nil)");
            break;
        }
        node = node->children[bit];
        log_append(bit ? " ->1" : " ->0");
        if (node->has_prefix) {
            format_cidr(node->prefix_ip, node->prefix_len, match_buf);
            best_match = match_buf;
            log_append("[");
            log_append(match_buf);
            log_append("]");
        }
    }
    log_flush();

    if (best_match) {
        str_copy(output_buf, best_match);
        log_begin();
        log_append("  match: ");
        log_append(output_buf);
        log_flush();
    } else {
        output_buf[0] = '\0';
        log_begin();
        log_append("  match: (none)");
        log_flush();
    }
    return output_buf;
}

// --- JSON serialization ---

static void serialize_node(TrieNode* node, const char* label, int depth) {
    json_putc('{');

    json_puts("\"bit\":");
    json_string(label);

    json_puts(",\"depth\":");
    json_int(depth);

    if (node->has_prefix) {
        json_puts(",\"prefix\":");
        char cidr[32];
        format_cidr(node->prefix_ip, node->prefix_len, cidr);
        json_string(cidr);
    } else {
        json_puts(",\"prefix\":null");
    }

    bool has_child0 = node->children[0] != nullptr;
    bool has_child1 = node->children[1] != nullptr;

    if (has_child0 || has_child1) {
        json_puts(",\"children\":[");
        bool first = true;
        if (has_child0) {
            serialize_node(node->children[0], "0", depth + 1);
            first = false;
        }
        if (has_child1) {
            if (!first) json_putc(',');
            serialize_node(node->children[1], "1", depth + 1);
        }
        json_putc(']');
    } else {
        json_puts(",\"children\":[]");
    }

    json_putc('}');
}

// --- Stats ---

static int count_nodes(TrieNode* node) {
    if (!node) return 0;
    return 1 + count_nodes(node->children[0]) + count_nodes(node->children[1]);
}

extern "C" int trie_node_count() {
    return count_nodes(root);
}

extern "C" int trie_node_size() {
    return sizeof(TrieNode);
}

extern "C" const char* trie_serialize() {
    json_ptr = output_buf;
    json_end = output_buf + sizeof(output_buf) - 1;

    if (!root) {
        json_puts("{}");
    } else {
        serialize_node(root, "root", 0);
    }
    *json_ptr = '\0';
    return output_buf;
}
