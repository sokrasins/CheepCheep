#ifndef WIEGANT_FMT_H_
#define WIEGAND_FMT_H_

#include <stdint.h>

// 24-bit wiegand format
#define WIEG_24BIT_TOTAL_BITS      26U
#define WIEG_24BIT_HIGH_MASK       0x01FFE000
#define WIEG_24BIT_LOW_MASK        0x00001FFE
#define WIEG_24BIT_FAC_MASK        0x01FE0000
#define WIEG_24BIT_FAC_OFFSET      17U
#define WIEG_24BIT_UID_MASK        0x0001FFFE
#define WIEG_24BIT_UID_OFFSET      1U
#define WIEG_24BIT_HIGH_PARITY_IDX 25U
#define WIEG_24BIT_LOW_PARITY_IDX  0U

// TODO: 32-bit format is broken. As-is, 32-bit mode requires the use of 
// uint64_t, which causes stack crashing in the door task. Implementing 32-bit 
// mode may require a refactor.
// 32-bit wiegand format
#define WIEG_32BIT_TOTAL_BITS      34U
#define WIEG_32BIT_HIGH_MASK       0xFFFE0000 
#define WIEG_32BIT_LOW_MASK        0x0001FFFE
#define WIEG_32BIT_FAC_MASK        0xFFFE0000
#define WIEG_32BIT_FAC_OFFSET      17U
#define WIEG_32BIT_UID_MASK        0x0001FFFE
#define WIEG_32BIT_UID_OFFSET      1U
#define WIEG_32BIT_HIGH_PARITY_IDX 33U
#define WIEG_32BIT_LOW_PARITY_IDX  0U

// High- and low-parity bit getters
#define WIEG_HIGH_PARITY_BIT(fmt, x)   ((x >> fmt->high_parity_idx) & 1)
#define WIEG_LOW_PARITY_BIT(fmt, x)    ((x >> fmt->low_parity_idx)  & 1)

// Format descriptor
typedef struct {
    uint32_t high_mask;     // Bits included in the high parity bit calculation
    uint32_t low_mask;      // Bits included in the low parity bit calculation
    uint32_t fac_mask;      // Bits included in facility
    uint32_t uid_mask;      // Bits included in the user id
    int total_bits;         // Total bits in card data (including parity)
    int fac_offset;         // Bit offset of facility code
    int uid_offset;         // Bit offset of user id
    int high_parity_idx;    // Bit index of high parity bit
    int low_parity_idx;     // Bit index of low parity bit
} wieg_fmt_desc_t;

// 26 and 34 bit formats
extern const wieg_fmt_desc_t wieg_fmt_24bit;
extern const wieg_fmt_desc_t wieg_fmt_32bit;

#endif /*WIEGAND_FMT_H_*/