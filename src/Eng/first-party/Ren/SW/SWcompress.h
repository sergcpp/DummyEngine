#ifndef SW_COMPRESS_H
#define SW_COMPRESS_H

#include "SWcore.h"

void swTexCompress(const void *data, SWenum mode, SWint w, SWint h, void **out_data,
                   SWint *out_size);
SWenum swTexDecompress(const void *data, SWint w, SWint h, void **out_data,
                       SWint *out_size);

#endif /* SW_COMPRESS_H */
