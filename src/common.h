#pragma once

using u32 = unsigned int;

// --- Shared allocator (defined in trie.cpp) ---
extern "C" void* alloc(int size);

// --- Compressed trie init (defined in ctrie.cpp, called by trie_init) ---
extern "C" void ctrie_init_internal();

// --- PATRICIA trie init (defined in ptrie.cpp, called by trie_init) ---
extern "C" void ptrie_init_internal();

// --- Minimal string utilities ---

static inline int str_len(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

// --- JS console.log bridge (imported from JS at WASM instantiation) ---
extern "C" void js_log(const char* ptr, int len);

static char log_buf[1024];
static int log_pos = 0;

static inline void log_begin() { log_pos = 0; }

static inline void log_append(const char* s) {
    while (*s && log_pos < 1023) log_buf[log_pos++] = *s++;
}

static inline void log_char(char c) {
    if (log_pos < 1023) log_buf[log_pos++] = c;
}

static inline void log_int(int val) {
    if (val < 0) { log_char('-'); val = -val; }
    char tmp[12]; int ti = 0;
    if (val == 0) { tmp[ti++] = '0'; }
    else { while (val > 0) { tmp[ti++] = '0' + val % 10; val /= 10; } }
    while (ti > 0) log_char(tmp[--ti]);
}

static inline void log_flush() {
    log_buf[log_pos] = '\0';
    js_log(log_buf, log_pos);
}

static inline void str_copy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = 0;
}

// --- IP / CIDR parsing ---

static inline bool parse_ip(const char* s, u32* out_ip, int* chars_consumed) {
    u32 ip = 0;
    int pos = 0;
    for (int octet = 0; octet < 4; octet++) {
        if (octet > 0) {
            if (s[pos] != '.') return false;
            pos++;
        }
        u32 val = 0;
        int digits = 0;
        while (s[pos] >= '0' && s[pos] <= '9') {
            val = val * 10 + (s[pos] - '0');
            pos++;
            digits++;
        }
        if (digits == 0 || val > 255) return false;
        ip = (ip << 8) | val;
    }
    *out_ip = ip;
    if (chars_consumed) *chars_consumed = pos;
    return true;
}

static inline bool parse_cidr(const char* s, u32* out_ip, int* out_prefix_len) {
    int consumed = 0;
    if (!parse_ip(s, out_ip, &consumed)) return false;

    if (s[consumed] != '/') return false;
    consumed++;

    int plen = 0;
    int digits = 0;
    while (s[consumed] >= '0' && s[consumed] <= '9') {
        plen = plen * 10 + (s[consumed] - '0');
        consumed++;
        digits++;
    }
    if (digits == 0 || plen > 32) return false;
    if (s[consumed] != '\0') return false;

    if (plen == 0)
        *out_ip = 0;
    else
        *out_ip = *out_ip & (0xFFFFFFFFu << (32 - plen));

    *out_prefix_len = plen;
    return true;
}

static inline void format_cidr(u32 ip, int prefix_len, char* buf) {
    char* p = buf;
    for (int i = 3; i >= 0; i--) {
        u32 octet = (ip >> (i * 8)) & 0xFF;
        if (octet >= 100) { *p++ = '0' + octet / 100; }
        if (octet >= 10)  { *p++ = '0' + (octet / 10) % 10; }
        *p++ = '0' + octet % 10;
        if (i > 0) *p++ = '.';
    }
    *p++ = '/';
    if (prefix_len >= 10) { *p++ = '0' + prefix_len / 10; }
    *p++ = '0' + prefix_len % 10;
    *p = '\0';
}

// --- Bit helpers ---

static inline int get_bit(u32 ip, int pos) {
    return (ip >> (31 - pos)) & 1;
}

// --- JSON helpers (each TU gets its own copy via static) ---

static char* json_ptr;
static char* json_end;

static inline void json_putc(char c) {
    if (json_ptr < json_end) *json_ptr++ = c;
}

static inline void json_puts(const char* s) {
    while (*s && json_ptr < json_end) *json_ptr++ = *s++;
}

static inline void json_string(const char* s) {
    json_putc('"');
    json_puts(s);
    json_putc('"');
}

static inline void json_int(int val) {
    char buf[12];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    else {
        char tmp[12]; int ti = 0;
        while (val > 0) { tmp[ti++] = '0' + val % 10; val /= 10; }
        while (ti > 0) buf[i++] = tmp[--ti];
    }
    buf[i] = '\0';
    json_puts(buf);
}
