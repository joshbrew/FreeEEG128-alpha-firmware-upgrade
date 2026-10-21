#ifndef FREEEEG128_UF2_LAYOUT_H
#define FREEEEG128_UF2_LAYOUT_H
#include <stdint.h>
#define UF2_BOOT_BASE       0x08000000u
#define UF2_APP_BASE        0x08020000u
#define UF2_APP_LIMIT       0x080e0000u
#define UF2_APP_META        0x080e0000u
#define UF2_STAGE_BASE      0x08100000u
#define UF2_STAGE_META      0x081e0000u
#define UF2_APP_MAX         (UF2_APP_LIMIT - UF2_APP_BASE)
#define UF2_SECTOR_SIZE     0x20000u
#define UF2_BLOCK_SIZE      256u
#define UF2_MAX_BLOCKS      (UF2_APP_MAX / UF2_BLOCK_SIZE)
#define UF2_FAMILY          0x6db66082u /* Microsoft's registered STM32H7 family */
#define UF2_BOARD           0x38314546u /* FE18 (128); requires our board manifest as well as family ID */
#define UF2_LAYOUT_VERSION  1u
#define UF2_RECORD_MAGIC    0x32474546u
#define UF2_BOARD_OFFSET    0x400u
#define UF2_BOARD_MAGIC     0x42474546u
typedef struct {
    uint32_t magic, layout, board, length, image_crc, length_inverse, crc_inverse, record_crc;
} uf2_record;
typedef struct {
    uint32_t magic, layout, board, app_base, app_limit, reserved[3];
} uf2_board_header;
#endif
