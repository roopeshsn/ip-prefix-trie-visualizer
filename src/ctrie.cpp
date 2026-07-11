#include "common.h"

// --- Compressed Binary Trie (Patricia Trie) ---
//
// Each node stores its absolute bit depth. Edges carry multi-bit labels.
// Only branching points and prefix-bearing nodes exist as actual nodes.

struct CTrieNode {
    CTrieNode* children[2];
    int depth;        // absolute bit position (0 for root)
    u32 edge_key;     // an IP that traverses the edge from parent to here
    bool has_prefix;
    u32 prefix_ip;
    int prefix_len;
};

static CTrieNode* croot = nullptr;

static CTrieNode* new_cnode() {
    CTrieNode* n = (CTrieNode*)alloc(sizeof(CTrieNode));
    if (!n) return nullptr;
    n->children[0] = nullptr;
    n->children[1] = nullptr;
    n->depth = 0;
    n->edge_key = 0;
    n->has_prefix = false;
    n->prefix_ip = 0;
    n->prefix_len = 0;
    return n;
}

// Find first bit position where two IPs differ, searching [from, to)
static int find_mismatch(u32 a, u32 b, int from, int to) {
    for (int i = from; i < to; i++) {
        if (get_bit(a, i) != get_bit(b, i)) return i;
    }
    return to; // no mismatch
}

// --- Init ---

extern "C" void ctrie_init_internal() {
    croot = new_cnode();
}

// --- Insert ---

extern "C" int ctrie_insert(const char* cidr) {
    log_begin(); log_append("[Compressed Trie] INSERT "); log_append(cidr); log_flush();
    u32 ip;
    int plen;
    if (!parse_cidr(cidr, &ip, &plen)) return -1;
    if (!croot) return -1;

    CTrieNode* node = croot;
    int cur_depth = 0;

    while (cur_depth < plen) {
        int bit = get_bit(ip, cur_depth);
        CTrieNode* child = node->children[bit];

        if (!child) {
            CTrieNode* leaf = new_cnode();
            if (!leaf) return -1;
            leaf->depth = plen;
            leaf->edge_key = ip;
            leaf->has_prefix = true;
            leaf->prefix_ip = ip;
            leaf->prefix_len = plen;
            node->children[bit] = leaf;
            return 0;
        }

        // Edge from cur_depth to child->depth
        // First bit (cur_depth) already matches via branch selection
        // Check remaining edge bits [cur_depth+1 .. min(child->depth, plen))
        int check_end = child->depth < plen ? child->depth : plen;
        int mismatch = find_mismatch(ip, child->edge_key, cur_depth + 1, check_end);

        if (mismatch < check_end) {
            // Split the edge at the mismatch point
            CTrieNode* split = new_cnode();
            if (!split) return -1;
            split->depth = mismatch;
            split->edge_key = ip;

            int split_bit = get_bit(child->edge_key, mismatch);
            split->children[split_bit] = child;
            node->children[bit] = split;

            if (plen == mismatch) {
                split->has_prefix = true;
                split->prefix_ip = ip;
                split->prefix_len = plen;
            } else {
                int new_bit = get_bit(ip, mismatch);
                CTrieNode* leaf = new_cnode();
                if (!leaf) return -1;
                leaf->depth = plen;
                leaf->edge_key = ip;
                leaf->has_prefix = true;
                leaf->prefix_ip = ip;
                leaf->prefix_len = plen;
                split->children[new_bit] = leaf;
            }
            return 0;
        }

        if (plen < child->depth) {
            // Prefix ends within this edge — split at plen
            CTrieNode* split = new_cnode();
            if (!split) return -1;
            split->depth = plen;
            split->edge_key = ip;
            split->has_prefix = true;
            split->prefix_ip = ip;
            split->prefix_len = plen;

            int split_bit = get_bit(child->edge_key, plen);
            split->children[split_bit] = child;
            node->children[bit] = split;
            return 0;
        }

        if (plen == child->depth) {
            child->has_prefix = true;
            child->prefix_ip = ip;
            child->prefix_len = plen;
            return 0;
        }

        // Full edge match, continue deeper
        cur_depth = child->depth;
        node = child;
    }

    // Landed exactly on an existing node
    node->has_prefix = true;
    node->prefix_ip = ip;
    node->prefix_len = plen;
    return 0;
}

// --- Delete ---

// Returns true if the node should be removed from its parent
static bool ctrie_delete_recursive(CTrieNode* parent, int parent_bit, u32 ip, int plen) {
    CTrieNode* node = parent->children[parent_bit];
    if (!node) return false;

    if (node->depth < plen) {
        // Continue deeper
        int next_bit = get_bit(ip, node->depth);
        CTrieNode* child = node->children[next_bit];
        if (!child) return false;

        // Verify edge bits match
        int check_end = child->depth < plen ? child->depth : plen;
        int mismatch = find_mismatch(ip, child->edge_key, node->depth + 1, check_end);
        if (mismatch < check_end) return false; // prefix not in trie

        bool child_removed = ctrie_delete_recursive(node, next_bit, ip, plen);

        if (child_removed) {
            node->children[next_bit] = nullptr;
        }

        // Merge if node now has exactly 1 child and no prefix
        if (!node->has_prefix) {
            CTrieNode* only_child = nullptr;
            if (node->children[0] && !node->children[1]) only_child = node->children[0];
            else if (node->children[1] && !node->children[0]) only_child = node->children[1];

            if (only_child) {
                // Merge: replace node with only_child in parent
                parent->children[parent_bit] = only_child;
            } else if (!node->children[0] && !node->children[1]) {
                return true; // remove this empty node
            }
        }
        return false;
    }

    if (node->depth == plen) {
        // Verify edge bits match
        int mismatch = find_mismatch(ip, node->edge_key, 0, plen);
        // Actually we need to check from the parent's perspective
        // The bits were already matched during traversal, so just unmark
        if (!node->has_prefix) return false;
        node->has_prefix = false;

        // If leaf (no children), remove it
        if (!node->children[0] && !node->children[1]) {
            return true;
        }

        // If single child, merge
        CTrieNode* only_child = nullptr;
        if (node->children[0] && !node->children[1]) only_child = node->children[0];
        else if (node->children[1] && !node->children[0]) only_child = node->children[1];

        if (only_child) {
            parent->children[parent_bit] = only_child;
        }
        return false;
    }

    return false;
}

extern "C" int ctrie_delete(const char* cidr) {
    log_begin(); log_append("[Compressed Trie] DELETE "); log_append(cidr); log_flush();
    u32 ip;
    int plen;
    if (!parse_cidr(cidr, &ip, &plen)) return -1;
    if (!croot) return -1;

    if (plen == 0) {
        // Special case: delete the /0 prefix from root
        croot->has_prefix = false;
        return 0;
    }

    int first_bit = get_bit(ip, 0);
    CTrieNode* child = croot->children[first_bit];
    if (!child) return 0;

    // Verify edge bits match from position 0
    int check_end = child->depth < plen ? child->depth : plen;
    int mismatch = find_mismatch(ip, child->edge_key, 1, check_end);
    if (mismatch < check_end) return 0; // prefix not found

    bool removed = ctrie_delete_recursive(croot, first_bit, ip, plen);
    if (removed) {
        croot->children[first_bit] = nullptr;
    }

    // Check if root's child can be merged (root always stays)
    return 0;
}

// --- Lookup ---

static char ctrie_lookup_buf[256];

extern "C" const char* ctrie_lookup(const char* ip_str) {
    u32 ip;
    int consumed;
    if (!parse_ip(ip_str, &ip, &consumed)) { ctrie_lookup_buf[0] = '\0'; return ctrie_lookup_buf; }
    if (ip_str[consumed] != '\0') { ctrie_lookup_buf[0] = '\0'; return ctrie_lookup_buf; }
    if (!croot) { ctrie_lookup_buf[0] = '\0'; return ctrie_lookup_buf; }

    log_begin(); log_append("[Compressed Trie] LOOKUP "); log_append(ip_str); log_flush();

    CTrieNode* node = croot;
    const char* best_match = nullptr;
    static char match_buf[64];
    char edge_label[33];

    log_begin(); log_append("  path: root");

    if (node->has_prefix) {
        format_cidr(node->prefix_ip, node->prefix_len, match_buf);
        best_match = match_buf;
    }

    int cur_depth = 0;
    while (cur_depth < 32 && node) {
        int bit = get_bit(ip, cur_depth);
        CTrieNode* child = node->children[bit];
        if (!child) {
            log_append(" -> (nil)");
            break;
        }

        // Format edge label
        log_append(" --(");
        for (int i = node->depth; i < child->depth && i < 32; i++) {
            log_char('0' + get_bit(child->edge_key, i));
        }
        log_append(")->");

        if (child->has_prefix) {
            format_cidr(child->prefix_ip, child->prefix_len, match_buf);
            best_match = match_buf;
            log_append("[");
            log_append(match_buf);
            log_append("]");
        } else {
            log_append("*");
        }

        cur_depth = child->depth;
        node = child;
    }
    log_flush();

    if (best_match) {
        str_copy(ctrie_lookup_buf, best_match);
        log_begin(); log_append("  match: "); log_append(ctrie_lookup_buf); log_flush();
    } else {
        ctrie_lookup_buf[0] = '\0';
        log_begin(); log_append("  match: (none)"); log_flush();
    }
    return ctrie_lookup_buf;
}

// --- Stats ---

static int count_cnodes(CTrieNode* node) {
    if (!node) return 0;
    return 1 + count_cnodes(node->children[0]) + count_cnodes(node->children[1]);
}

extern "C" int ctrie_node_count() {
    return count_cnodes(croot);
}

extern "C" int ctrie_node_size() {
    return sizeof(CTrieNode);
}

// --- JSON serialization ---

static char ctrie_output_buf[64 * 1024];

static void format_edge_label(u32 edge_key, int from_depth, int to_depth, char* buf) {
    int len = to_depth - from_depth;
    for (int i = 0; i < len; i++) {
        buf[i] = '0' + get_bit(edge_key, from_depth + i);
    }
    buf[len] = '\0';
}

static void serialize_cnode(CTrieNode* node, const char* label, int parent_depth) {
    json_putc('{');

    json_puts("\"bit\":");
    json_string(label);

    json_puts(",\"depth\":");
    json_int(node->depth);

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
            char edge_label[33];
            format_edge_label(node->children[0]->edge_key, node->depth, node->children[0]->depth, edge_label);
            serialize_cnode(node->children[0], edge_label, node->depth);
            first = false;
        }
        if (has_child1) {
            if (!first) json_putc(',');
            char edge_label[33];
            format_edge_label(node->children[1]->edge_key, node->depth, node->children[1]->depth, edge_label);
            serialize_cnode(node->children[1], edge_label, node->depth);
        }
        json_putc(']');
    } else {
        json_puts(",\"children\":[]");
    }

    json_putc('}');
}

extern "C" const char* ctrie_serialize() {
    json_ptr = ctrie_output_buf;
    json_end = ctrie_output_buf + sizeof(ctrie_output_buf) - 1;

    if (!croot) {
        json_puts("{}");
    } else {
        serialize_cnode(croot, "root", 0);
    }
    *json_ptr = '\0';
    return ctrie_output_buf;
}
