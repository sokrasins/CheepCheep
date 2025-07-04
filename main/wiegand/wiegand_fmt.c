#include "wiegand_fmt.h"

// 24-bit format descriptor
const wieg_fmt_desc_t wieg_fmt_24bit = {
    .high_mask       = WIEG_24BIT_HIGH_MASK,
    .low_mask        = WIEG_24BIT_LOW_MASK,
    .fac_mask        = WIEG_24BIT_FAC_MASK,
    .uid_mask        = WIEG_24BIT_UID_MASK,
    .total_bits      = WIEG_24BIT_TOTAL_BITS,
    .fac_offset      = WIEG_24BIT_FAC_OFFSET,
    .uid_offset      = WIEG_24BIT_UID_OFFSET,
    .high_parity_idx = WIEG_24BIT_HIGH_PARITY_IDX,
    .low_parity_idx  = WIEG_24BIT_LOW_PARITY_IDX,
};

// 32-bit format descriptor
const wieg_fmt_desc_t wieg_fmt_32bit = {
    .high_mask       = WIEG_32BIT_HIGH_MASK,
    .low_mask        = WIEG_32BIT_LOW_MASK,
    .fac_mask        = WIEG_32BIT_FAC_MASK,
    .uid_mask        = WIEG_32BIT_UID_MASK,
    .total_bits      = WIEG_32BIT_TOTAL_BITS,
    .fac_offset      = WIEG_32BIT_FAC_OFFSET,
    .uid_offset      = WIEG_32BIT_UID_OFFSET,
    .high_parity_idx = WIEG_32BIT_HIGH_PARITY_IDX,
    .low_parity_idx  = WIEG_32BIT_LOW_PARITY_IDX,
};