// SPDX-License-Identifier: MIT
// PNM formats decoder
// Copyright (C) 2023 Abe Wieland <abe.wieland@gmail.com>

#include "../loader.h"

#include <limits.h>
#include <bzlib.h>
#include <stdlib.h>

enum loader_status decode_bz2(struct image* ctx, const uint8_t* data,
                              size_t size)
{
    unsigned int decsize = 2 * (unsigned int) size;
    char* decdata = (char*)malloc(sizeof(char) * (int) decsize);
    while ( true ) {
        switch (BZ2_bzBuffToBuffDecompress(decdata, &decsize, (char *) data, (unsigned int) size, 0, 0)){
            case BZ_MEM_ERROR:
                free(decdata);
                if ( decsize > INT_MAX/2 ) return ldr_fmterror;
                decsize*=2;
                decdata = (char*)malloc(sizeof(char) * (int) decsize);
                continue;
            case BZ_DATA_ERROR_MAGIC:
                free(decdata);
                return ldr_unsupported;
            case BZ_OK:
                break;
            default:
                free(decdata);
                return ldr_fmterror;
        }
        break;
    }
    enum loader_status recursive_status = recur_loader(ctx, (const uint8_t *) decdata, (size_t) decsize);
    if ( recursive_status != ldr_success ) return recursive_status;
    image_set_format(ctx, ctx->format, "+bzip2");
    return ldr_success;
}
