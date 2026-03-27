#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} line_proto_t;

void line_proto_init(line_proto_t *p, char *storage, size_t storage_sz);
bool line_proto_feed(line_proto_t *p, uint8_t byte);   // מחזיר true כשיש שורה מלאה
const char* line_proto_get_line(line_proto_t *p);      // מצביע לשורה (null-terminated)
void line_proto_reset(line_proto_t *p);