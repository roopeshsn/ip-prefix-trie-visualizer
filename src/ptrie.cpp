#include "common.h"

// --- PATRICIA Trie (with back-pointers) adapted for IP prefixes ---

struct PTrieNode {
    PTrieNode* children[2];
    int bit_index;
    u32 key;
    int prefix_len;
    bool has_prefix;
    int id;
};

static PTrieNode* proot = nullptr;
static int pnode_count = 0;

static PTrieNode* new_pnode(int bit_idx, u32 k, int plen, bool has_pfx) {
    PTrieNode* n = (PTrieNode*)alloc(sizeof(PTrieNode));
    if (!n) return nullptr;
    n->children[0] = nullptr;
    n->children[1] = nullptr;
    n->bit_index = bit_idx;
    n->key = k;
    n->prefix_len = plen;
    n->has_prefix = has_pfx;
    n->id = -1;
    pnode_count++;
    return n;
}

static int first_differing_bit(u32 a, u32 b) {
    u32 diff = a ^ b;
    if (diff == 0) return 32;
    int pos = 0;
    if ((diff & 0xFFFF0000u) == 0) { pos += 16; diff <<= 16; }
    if ((diff & 0xFF000000u) == 0) { pos += 8; diff <<= 8; }
    if ((diff & 0xF0000000u) == 0) { pos += 4; diff <<= 4; }
    if ((diff & 0xC0000000u) == 0) { pos += 2; diff <<= 2; }
    if ((diff & 0x80000000u) == 0) { pos += 1; }
    return pos;
}

static bool is_forward_edge(PTrieNode* parent, PTrieNode* child) {
    return child && child->bit_index > parent->bit_index;
}

// --- Init ---

extern "C" void ptrie_init_internal() {
    proot = new_pnode(-1, 0, 0, false);
    if (proot) {
        proot->children[0] = proot;
        proot->children[1] = proot;
    }
    pnode_count = 1;
}

// --- Insert ---

extern "C" int ptrie_insert(const char* cidr) {
    log_begin(); log_append("[PATRICIA Trie] INSERT "); log_append(cidr); log_flush();
    u32 ip;
    int plen;
    if (!parse_cidr(cidr, &ip, &plen)) return -1;
    if (!proot) return -1;

    // Phase 1: Search — follow until back-pointer, track last real node
    PTrieNode* prev = proot;
    PTrieNode* cur = proot->children[0];
    PTrieNode* last_real = nullptr;

    while (is_forward_edge(prev, cur)) {
        last_real = cur;
        prev = cur;
        int bit = get_bit(ip, cur->bit_index);
        cur = cur->children[bit];
    }

    // Use the real node's key for comparison (not the sentinel's key=0)
    PTrieNode* compare_node = cur;
    if (cur == proot && last_real) compare_node = last_real;

    // Check for duplicate
    if (compare_node->has_prefix && compare_node->key == ip && compare_node->prefix_len == plen) {
        return 0;
    }

    // Phase 2: Find first differing bit
    int d = first_differing_bit(ip, compare_node->key);

    if (d >= 32) {
        // Same key — just mark as prefix
        compare_node->has_prefix = true;
        compare_node->key = ip;
        compare_node->prefix_len = plen;
        return 0;
    }

    // Phase 3: Re-traverse to find insertion point
    prev = proot;
    cur = proot->children[0];

    while (is_forward_edge(prev, cur) && cur->bit_index < d) {
        prev = cur;
        int bit = get_bit(ip, cur->bit_index);
        cur = cur->children[bit];
    }

    // Insert new node between prev and cur
    PTrieNode* node = new_pnode(d, ip, plen, true);
    if (!node) return -1;

    int new_bit = get_bit(ip, d);
    node->children[new_bit] = node;        // back-pointer to self
    node->children[1 - new_bit] = cur;     // other side → old target

    // Re-link parent
    if (prev == proot) {
        proot->children[0] = node;
    } else {
        int prev_bit = get_bit(ip, prev->bit_index);
        prev->children[prev_bit] = node;
    }

    return 0;
}

// --- Delete (simplified: unmark prefix) ---

extern "C" int ptrie_delete(const char* cidr) {
    log_begin(); log_append("[PATRICIA Trie] DELETE "); log_append(cidr); log_flush();
    u32 ip;
    int plen;
    if (!parse_cidr(cidr, &ip, &plen)) return -1;
    if (!proot) return -1;

    // Search for the node with this prefix
    PTrieNode* prev = proot;
    PTrieNode* cur = proot->children[0];

    while (is_forward_edge(prev, cur)) {
        prev = cur;
        int bit = get_bit(ip, cur->bit_index);
        cur = cur->children[bit];
    }

    if (cur->has_prefix && cur->key == ip && cur->prefix_len == plen) {
        cur->has_prefix = false;
    }

    return 0;
}

// --- Stats ---

// --- Lookup ---

static char ptrie_lookup_buf[256];

static bool prefix_matches(u32 search_ip, u32 prefix_ip, int prefix_len) {
    if (prefix_len == 0) return true;
    u32 mask = 0xFFFFFFFFu << (32 - prefix_len);
    return (search_ip & mask) == (prefix_ip & mask);
}

extern "C" const char* ptrie_lookup(const char* ip_str) {
    u32 ip;
    int consumed;
    if (!parse_ip(ip_str, &ip, &consumed)) { ptrie_lookup_buf[0] = '\0'; return ptrie_lookup_buf; }
    if (ip_str[consumed] != '\0') { ptrie_lookup_buf[0] = '\0'; return ptrie_lookup_buf; }
    if (!proot) { ptrie_lookup_buf[0] = '\0'; return ptrie_lookup_buf; }

    log_begin(); log_append("[PATRICIA Trie] LOOKUP "); log_append(ip_str); log_flush();

    const char* best_match = nullptr;
    static char match_buf[64];

    log_begin(); log_append("  path: root(bit_index=-1)");

    PTrieNode* prev = proot;
    PTrieNode* cur = proot->children[0];

    while (is_forward_edge(prev, cur)) {
        log_append(" --fwd(bit ");
        log_int(cur->bit_index);
        log_append(")-> ");

        if (cur->has_prefix && prefix_matches(ip, cur->key, cur->prefix_len)) {
            format_cidr(cur->key, cur->prefix_len, match_buf);
            best_match = match_buf;
            log_append("[");
            log_append(match_buf);
            log_append("]");
        } else {
            log_append("*");
        }

        prev = cur;
        int bit = get_bit(ip, cur->bit_index);
        cur = cur->children[bit];
    }

    // Back-pointer reached
    log_append(" --back-> ");
    if (cur->has_prefix) {
        char cidr[32];
        format_cidr(cur->key, cur->prefix_len, cidr);
        log_append("[");
        log_append(cidr);
        log_append("]");
        if (prefix_matches(ip, cur->key, cur->prefix_len)) {
            if (!best_match || cur->prefix_len > 0) {
                format_cidr(cur->key, cur->prefix_len, match_buf);
                best_match = match_buf;
            }
        }
    } else {
        log_append("(no prefix)");
    }
    log_flush();

    if (best_match) {
        str_copy(ptrie_lookup_buf, best_match);
        log_begin(); log_append("  match: "); log_append(ptrie_lookup_buf); log_flush();
    } else {
        ptrie_lookup_buf[0] = '\0';
        log_begin(); log_append("  match: (none)"); log_flush();
    }
    return ptrie_lookup_buf;
}

// Collect all unique nodes reachable from proot
static PTrieNode* all_nodes[1024];
static int all_node_count;

static bool node_seen(PTrieNode* n) {
    for (int i = 0; i < all_node_count; i++) {
        if (all_nodes[i] == n) return true;
    }
    return false;
}

static void collect_all_nodes(PTrieNode* node) {
    if (!node || node_seen(node) || all_node_count >= 1024) return;
    all_nodes[all_node_count++] = node;
    collect_all_nodes(node->children[0]);
    collect_all_nodes(node->children[1]);
}

extern "C" int ptrie_node_count() {
    all_node_count = 0;
    collect_all_nodes(proot);
    return all_node_count;
}

extern "C" int ptrie_node_size() {
    return sizeof(PTrieNode);
}

// --- Serialization (graph format) ---

static char ptrie_output_buf[64 * 1024];

// Assign IDs to all reachable nodes
static void assign_all_ids() {
    all_node_count = 0;
    collect_all_nodes(proot);
    for (int i = 0; i < all_node_count; i++) {
        all_nodes[i]->id = i;
    }
}

// Compute forward-edge depth for each node (for layout positioning)
static int node_depth[1024];

static void compute_depths(PTrieNode* node, PTrieNode* parent, int depth) {
    if (!node || !is_forward_edge(parent, node)) return;
    if (node->id >= 0 && node->id < 1024) {
        node_depth[node->id] = depth;
    }
    compute_depths(node->children[0], node, depth + 1);
    compute_depths(node->children[1], node, depth + 1);
}

extern "C" const char* ptrie_serialize() {
    json_ptr = ptrie_output_buf;
    json_end = ptrie_output_buf + sizeof(ptrie_output_buf) - 1;

    if (!proot) {
        json_puts("{}");
        *json_ptr = '\0';
        return ptrie_output_buf;
    }

    assign_all_ids();

    // Compute depths for layout
    for (int i = 0; i < 1024; i++) node_depth[i] = -1;
    node_depth[proot->id] = 0;
    compute_depths(proot->children[0], proot, 1);
    compute_depths(proot->children[1], proot, 1);

    // Nodes that are back-pointer-only targets get depth = referencing node's depth + 1
    for (int i = 0; i < all_node_count; i++) {
        if (node_depth[all_nodes[i]->id] < 0) {
            // Find a node that references this one
            for (int j = 0; j < all_node_count; j++) {
                if (all_nodes[j]->children[0] == all_nodes[i] ||
                    all_nodes[j]->children[1] == all_nodes[i]) {
                    int ref_depth = node_depth[all_nodes[j]->id];
                    if (ref_depth >= 0) {
                        node_depth[all_nodes[i]->id] = ref_depth + 1;
                        break;
                    }
                }
            }
            if (node_depth[all_nodes[i]->id] < 0)
                node_depth[all_nodes[i]->id] = 0;
        }
    }

    // Output: {"nodes":[...], "edges":[...]}
    json_puts("{\"nodes\":[");
    for (int i = 0; i < all_node_count; i++) {
        if (i > 0) json_putc(',');
        PTrieNode* n = all_nodes[i];
        json_putc('{');
        json_puts("\"id\":");
        json_int(n->id);
        json_puts(",\"bit_index\":");
        if (n->bit_index < 0) json_puts("-1");
        else json_int(n->bit_index);
        json_puts(",\"depth\":");
        json_int(node_depth[n->id]);
        if (n->has_prefix) {
            json_puts(",\"prefix\":");
            char cidr[32];
            format_cidr(n->key, n->prefix_len, cidr);
            json_string(cidr);
        } else {
            json_puts(",\"prefix\":null");
        }
        json_puts(",\"is_root\":");
        json_puts(n == proot ? "true" : "false");
        json_putc('}');
    }

    json_puts("],\"edges\":[");
    bool first_edge = true;
    for (int i = 0; i < all_node_count; i++) {
        PTrieNode* n = all_nodes[i];
        for (int b = 0; b < 2; b++) {
            PTrieNode* child = n->children[b];
            if (!child) continue;
            // Skip sentinel self-loops and back-edges targeting the sentinel
            if (n == proot && child == proot) continue;
            if (!is_forward_edge(n, child) && child == proot) continue;
            bool forward = is_forward_edge(n, child);
            if (!first_edge) json_putc(',');
            first_edge = false;
            json_putc('{');
            json_puts("\"from\":");
            json_int(n->id);
            json_puts(",\"to\":");
            json_int(child->id);
            json_puts(",\"bit\":\"");
            json_putc('0' + b);
            json_puts("\",\"type\":\"");
            json_puts(forward ? "forward" : "back");
            json_puts("\"}");
        }
    }

    json_puts("]}");
    *json_ptr = '\0';
    return ptrie_output_buf;
}
