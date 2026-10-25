#ifndef HF_HEATSINK_DATA_TRANSACTION_H
#define HF_HEATSINK_DATA_TRANSACTION_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int (*HfRowRead)(void*,uintptr_t,void*,size_t);
typedef int (*HfRowWrite)(void*,uintptr_t,const void*,size_t);
/* Restore only changed bytes that still equal our intended value. Foreign
 * bytes elsewhere in a row do not prevent earlier owned writes being undone. */
static int hf_row_undo_owned(HfRowRead read,HfRowWrite write,void* context,uintptr_t address,
    const uint8_t* before,const uint8_t* after,size_t size) {
    int restored=1;
    if (!read || !write || !before || !after || size>4872) return 0;
    for (size_t i=0;i<size;i+=4) {
        size_t n=size-i<4?size-i:4;uint8_t current[4];int foreign=0;
        if (!memcmp(before+i,after+i,n)) continue;
        if (read(context,address+i,current,n)) { restored=0;continue; }
        for (size_t j=0;j<n;j++) if (current[j]!=before[i+j] && current[j]!=after[i+j]) foreign=1;
        /* Preserve a foreign scalar as a whole, including coincident bytes. */
        if (foreign || !memcmp(current,before+i,n)) continue;
        if (write(context,address+i,before+i,n) || read(context,address+i,current,n) || memcmp(current,before+i,n)) restored=0;
    }
    return restored;
}
/* 1 committed, 0 refused/repaired, -1 owned bytes could not be repaired. */
static int hf_row_write_if(HfRowRead read,HfRowWrite write,void* context,uintptr_t address,
    const uint8_t* before,const uint8_t* after,size_t size) {
    uint8_t actual[4872];
    if (!read || !write || !before || !after || !size || size>sizeof(actual) ||
        read(context,address,actual,size) || memcmp(actual,before,size)) return 0;
    int written=write(context,address,after,size)==0;
    if (written && !read(context,address,actual,size) && !memcmp(actual,after,size)) return 1;
    return hf_row_undo_owned(read,write,context,address,before,after,size)?0:-1;
}
#endif
