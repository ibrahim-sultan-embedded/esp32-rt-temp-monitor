#include "line_proto.h"

void line_proto_init(line_proto_t *p, char *storage, size_t storage_sz)
{
    p->buf = storage;
    p->cap = storage_sz;
    p->len = 0;
    if (p->cap) p->buf[0] = '\0';
}

bool line_proto_feed(line_proto_t *p, uint8_t byte)
{
    if (!p || !p->buf || p->cap == 0) return false;

    // התעלמות מ-CR
    if (byte == '\r') return false;

    if (byte == '\n') {
        if (p->len < p->cap) p->buf[p->len] = '\0';
        return true;
    }

    if (p->len + 1 < p->cap) {
        p->buf[p->len++] = (char)byte;
        p->buf[p->len] = '\0';
    } else {
        // overflow -> reset (כמו “defensive programming”)
        p->len = 0;
        p->buf[0] = '\0';
    }
    return false;
}

const char* line_proto_get_line(line_proto_t *p)
{
    return (p && p->buf) ? p->buf : "";
}

void line_proto_reset(line_proto_t *p)
{
    if (!p || !p->buf) return;
    p->len = 0;
    p->buf[0] = '\0';
}