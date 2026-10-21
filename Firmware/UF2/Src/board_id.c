#include "uf2_layout.h"
/* The linker fixes this at APP_BASE + 0x400, after the exception vector table. */
__attribute__((used, section(".board_id")))
const uf2_board_header freeeeg128_board = {
    UF2_BOARD_MAGIC, UF2_LAYOUT_VERSION, UF2_BOARD, UF2_APP_BASE, UF2_APP_LIMIT, {0, 0, 0}
};
