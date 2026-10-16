#include <stdint.h>

volatile uint64_t orion_toolchain_probe_state = UINT64_C(0x4f52494f4e);

__attribute__((used))
uint64_t orion_toolchain_probe(uint64_t input)
{
    return (input ^ orion_toolchain_probe_state) + UINT64_C(0x1234);
}
