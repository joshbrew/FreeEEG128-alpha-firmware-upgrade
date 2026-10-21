/* Bounded newlib heap. libnosys' default _sbrk always fails, which prevents both
   the USB class and ADC device structures from being allocated. */
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

void *_sbrk(ptrdiff_t increment)
{
    extern uint8_t _end, _estack;
    static uint8_t *next;
    uint8_t *previous;
    uintptr_t limit = (uintptr_t)&_estack - 32768u;
    if (!next) next = &_end;
    if (increment < 0 || (uintptr_t)next > limit || (uintptr_t)increment > limit - (uintptr_t)next) {
        errno = ENOMEM;
        return (void *)-1;
    }
    previous = next;
    next += increment;
    return previous;
}
