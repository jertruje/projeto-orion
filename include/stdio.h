#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct _IO_FILE FILE;

struct _IO_FILE {
    int dummy; // Just to make it a complete type
};

extern FILE *stderr;

int fprintf(FILE *stream, const char *format, ...);
int puts(const char *s);

#endif
