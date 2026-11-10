/* -*- C++ -*-
 * Copyright 2019-2026 LibRaw LLC (info@libraw.org)
 *
 LibRaw uses code from dcraw.c -- Dave Coffin's raw photo decoder,
 dcraw.c is copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net.
 LibRaw do not use RESTRICTED code from dcraw.c

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"
#include "alpa_phaseone_black.h"


void LibRaw::phase_one_allocate_tempbuffer()
{
  // Allocate temp raw_image buffer
	if (imgdata.process_warnings & LIBRAW_WARN_RAWSPEED3_PROCESSED)
	{
		// Processed by RawSpeed, raw_image contains ptr to rawspeed internals
		imgdata.rawdata.raw_alloc = imgdata.rawdata.raw_image; // save for data processing and future reuse
	}
    imgdata.rawdata.raw_image = (ushort *)malloc(UINT64(S.raw_pitch) * UINT64(S.raw_height));
}
void LibRaw::phase_one_free_tempbuffer()
{
	free(imgdata.rawdata.raw_image);
    imgdata.rawdata.raw_image = (ushort *)imgdata.rawdata.raw_alloc;
	if (imgdata.process_warnings & LIBRAW_WARN_RAWSPEED3_PROCESSED)
		imgdata.rawdata.raw_alloc = 0;
}

// ALPA patch(暗场降噪 IIQ 黑电平,2026-11-07):判据与来由见同目录 alpa_phaseone_black.h(与 macOS 转换器共用一份)。

int LibRaw::phase_one_subtract_black(ushort *src, ushort *dest)
{

  try
  {
    if (O.user_black < 0 && O.user_cblack[0] <= -1000000 &&
        O.user_cblack[1] <= -1000000 && O.user_cblack[2] <= -1000000 &&
        O.user_cblack[3] <= -1000000)
    {
      const int alpaBlack = alpa_p1_measured_black(src, S.raw_width, S.raw_height, S.top_margin, S.left_margin,
                                                   S.width, S.height, imgdata.color.phase_one_data.t_black);
      if (!imgdata.rawdata.ph1_cblack || !imgdata.rawdata.ph1_rblack)
      {
        int bl = alpaBlack;
        for (int row = 0; row < S.raw_height; row++)
        {
          checkCancel();
          for (int col = 0; col < S.raw_width; col++)
          {
            int idx = row * S.raw_width + col;
            int val = int(src[idx]) - bl;
            dest[idx] = val > 0 ? val : 0;
          }
        }
      }
      else
      {
        int bl = alpaBlack;
        // ALPA patch(IIQ 并行,2026-10-24):逐像素独立 ⟹ 按 64 行一块检查取消、块内按行并行;结果逐位一致。
        for (int r0 = 0; r0 < S.raw_height; r0 += 64)
        {
          checkCancel();
          const int r1 = MIN(int(S.raw_height), r0 + 64);
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(static)
#endif
        for (int row = r0; row < r1; row++)
        {
          for (int col = 0; col < S.raw_width; col++)
          {
            int idx = row * S.raw_width + col;
            int val =
                int(src[idx]) - bl +
                imgdata.rawdata
                    .ph1_cblack[row][col >= imgdata.rawdata.color.phase_one_data
                                                .split_col] +
                imgdata.rawdata
                    .ph1_rblack[col][row >= imgdata.rawdata.color.phase_one_data
                                                .split_row];
            dest[idx] = val > 0 ? val : 0;
          }
        }
        }
      }
    }
    else // black set by user interaction
    {
      // Black level in cblack!
      for (int row = 0; row < S.raw_height; row++)
      {
        checkCancel();
        unsigned short cblk[16];
        for (int cc = 0; cc < 16; cc++)
          cblk[cc] = C.cblack[fcol(row, cc)];
        for (int col = 0; col < S.raw_width; col++)
        {
          int idx = row * S.raw_width + col;
          ushort val = src[idx];
          ushort bl = cblk[col & 0xf];
          dest[idx] = val > bl ? val - bl : 0;
        }
      }
    }
    return 0;
  }
  catch (const LibRaw_exceptions& )
  {
    return LIBRAW_CANCELLED_BY_CALLBACK;
  }
}
