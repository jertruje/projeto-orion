#ifndef ORION_PANIC_H
#define ORION_PANIC_H

_Noreturn void orion_panic_at(const char *module, const char *reason);
_Noreturn void orion_panic(const char *reason);

#endif
