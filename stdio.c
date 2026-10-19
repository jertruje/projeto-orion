#include <stddef.h>
#include <stdarg.h>

typedef struct _IO_FILE {
    int dummy;
} FILE;

FILE *stderr = &(FILE){0};

int fprintf(FILE *stream, const char *format, ...)
{
    (void)stream;
    (void)format;
    va_list args;
    va_start(args, format);
    va_end(args);
    return 0;
}

int puts(const char *s)
{
    (void)s;
    return 0;
}
