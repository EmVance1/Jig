#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>


char* read_to_string(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    const size_t size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)calloc(size + 1, sizeof(char));
    fread(buf, sizeof(char), size, f);
    fclose(f);
    return buf;
}

