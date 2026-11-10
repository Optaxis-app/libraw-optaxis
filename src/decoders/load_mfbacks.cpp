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

#include "../../internal/dcraw_defs.h"
#include <vector>

inline uint32_t abs32(int32_t x)
{
  // Branchless version.
  uint32_t sm = x >> 31;
  return (uint32_t) ((x + sm) ^ sm);
}

inline uint32_t min32(uint32_t x, uint32_t y)
{
  return x < y ? x : y;
}

inline uint32_t max32(uint32_t x, uint32_t y)
{
  return x > y ? x : y;
}

inline uint32_t constain32(uint32_t x, uint32_t l, uint32_t u)
{
  return x < l ? l : (x > u ? u : x);
}

int unsigned_cmp(const void *a, const void *b)
{
  if (!a || !b)
    return 0;

  return *(unsigned *)a > *(unsigned *)b ? 1 : (*(unsigned *)a < *(unsigned *)b ? -1 : 0);
}

int LibRaw::p1rawc(unsigned row, unsigned col, unsigned& count)
{
  return (row < raw_height && col < raw_width) ? (++count, RAW(row, col)) : 0;
}

int LibRaw::p1raw(unsigned row, unsigned col)
{
  return (row < raw_height && col < raw_width) ? RAW(row, col) : 0;
}


// DNG SDK version of fixing pixels in bad column using averages sets
// corrected not to use pixels in the same column
void LibRaw::phase_one_fix_col_pixel_avg(unsigned row, unsigned col)
{
  static const int8_t dir[3][8][2] = {
  { {-2,-2}, {-2, 2}, {2,-2}, {2, 2}, { 0, 0}, { 0, 0}, {0, 0}, {0, 0} },
  { {-2,-4}, {-4,-2}, {2,-4}, {4,-2}, {-2, 4}, {-4, 2}, {2, 4}, {4, 2} },
  { {-4,-4}, {-4, 4}, {4,-4}, {4, 4}, { 0, 0}, { 0, 0}, {0, 0}, {0, 0} } };

  for (int set=0; set < 3; ++set)
  {
    uint32_t total = 0;
    uint32_t count = 0;
    for (int i = 0; i < 8; ++i)
    {
      if (!dir[set][i][0] && !dir[set][i][1])
        break;

      total += p1rawc(row+dir[set][i][0], col+dir[set][i][1], count);
    }

    if (count)
    {
      RAW(row,col) = (uint16_t)((total + (count >> 1)) / count);
      break;
    }
  }
}

// DNG SDK version of fixing pixels in bad column using gradient prediction
void LibRaw::phase_one_fix_pixel_grad(unsigned row, unsigned col)
{
  static const int8_t grad_sets[7][12][2] = {
    { {-4,-2}, { 4, 2}, {-3,-1}, { 1, 1}, {-1,-1}, { 3, 1}, 
      {-4,-1}, { 0, 1}, {-2,-1}, { 2, 1}, { 0,-1}, { 4, 1} },
    { {-2,-2}, { 2, 2}, {-3,-1}, {-1, 1}, {-1,-1}, { 1, 1}, 
      { 1,-1}, { 3, 1}, {-2,-1}, { 0, 1}, { 0,-1}, { 2, 1} },
    { {-2,-4}, { 2, 4}, {-1,-3}, { 1, 1}, {-1,-1}, { 1, 3}, 
      {-2,-1}, { 0, 3}, {-1,-2}, { 1, 2}, { 0,-3}, { 2, 1} },
    { { 0,-2}, { 0, 2}, {-1,-1}, {-1, 1}, { 1,-1}, { 1, 1}, 
      {-1,-2}, {-1, 2}, { 0,-1}, { 0,-1}, { 1,-2}, { 1, 2} },
    { {-2, 4}, { 2,-4}, {-1, 3}, { 1,-1}, {-1, 1}, { 1,-3}, 
      {-2, 1}, { 0,-3}, {-1, 2}, { 1,-2}, { 0, 3}, { 2,-1} },
    { {-2, 2}, { 2,-2}, {-3, 1}, {-1,-1}, {-1, 1}, { 1,-1}, 
      { 1, 1}, { 3,-1}, {-2, 1}, { 0,-1}, { 0, 1}, { 2,-1} },
    { {-4, 2}, { 4,-2}, {-3, 1}, { 1,-1}, {-1, 1}, { 3,-1}, 
      {-4, 1}, { 0,-1}, {-2, 1}, { 2,-1}, { 0, 1}, { 4,-1} } };

  uint32_t est[7], grad[7];
  uint32_t lower = min32(p1raw(row,col-2), p1raw(row, col+2));
  uint32_t upper = max32(p1raw(row,col-2), p1raw(row, col+2));
  uint32_t minGrad = 0xFFFFFFFF;
  for (int i = 0; i<7; ++i)
  {
    est[i] = p1raw(row+grad_sets[i][0][0], col+grad_sets[i][0][1]) +
             p1raw(row+grad_sets[i][1][0], col+grad_sets[i][1][1]);
    grad[i] = 0;
    for (int j=0; j<12; j+=2)
      grad[i] += abs32(p1raw(row+grad_sets[i][j][0], col+grad_sets[i][j][1]) -
                       p1raw(row+grad_sets[i][j+1][0], col+grad_sets[i][j+1][1]));
    minGrad = min32(minGrad, grad[i]);
  }

  uint32_t limit = (minGrad * 3) >> 1;
  uint32_t total = 0;
  uint32_t count = 0;
  for (int i = 0; i<7; ++i)
    if (grad[i] <= limit)
    {
      total += est[i];
      count += 2;
    }
  RAW(row, col) = constain32((total + (count >> 1)) / count, lower, upper);
}

void LibRaw::phase_one_flat_field(int is_float, int nc)
{
  ushort head[8];
  unsigned wide, high, y, x, c, rend, cend, row, col;
  float *mrow, num, mult[4];

  read_shorts(head, 8);
  if (head[2] == 0 || head[3] == 0 || head[4] == 0 || head[5] == 0)
    return;
  wide = head[2] / head[4] + (head[2] % head[4] != 0);
  high = head[3] / head[5] + (head[3] % head[5] != 0);
  mrow = (float *)calloc(nc * wide, sizeof *mrow);
  for (y = 0; y < high; y++)
  {
    checkCancel();
    for (x = 0; x < wide; x++)
      for (c = 0; c < (unsigned)nc; c += 2)
      {
        num = is_float ? getrealf(LIBRAW_EXIFTAG_TYPE_FLOAT) : float(get2()) / 32768.f;
        if (y == 0)
          mrow[c * wide + x] = num;
        else
          mrow[(c + 1) * wide + x] = (num - mrow[c * wide + x]) / head[5];
      }
    if (y == 0)
      continue;
    rend = head[1] + y * head[5];
    const unsigned rstart = rend - head[5];
    unsigned rstop = rstart;
    while (rstop < raw_height && rstop < rend && rstop < unsigned(head[1] + head[3] - head[5]))
      rstop++;
    // ALPA patch(IIQ 并行,2026-10-24 → 2026-10-29 改成按网格带):原版逐行串行、行内各网格列互不依赖。
    // 10-24 版在每一行里开一次并行区(一张 IQ4 是 1 万多行 × 两次平场),fork/join 吃掉了收益:
    // arm64 Mac 上 1 线程与 4 线程同为约 2.2 s。改后 S26 Ultra 实测(IQ4 150MP 半尺寸,4 线程)
    // open 到 scale_colors 之前 0.63–0.69 s → 0.58 s,整次 LibRaw 约 1.08 s → 0.98 s。
    // 现在每个网格带只开一次:每个格列 xi 在本带所有行上独立推进,它读的两列 mrow(xi-1、xi)
    // 在线程内用本地副本按原顺序做同样的 m += d;带结束后再由主线程对 mrow 做同样次数的累加。
    // 每个像素的算式、乘子与浮点累加顺序都与原版相同 ⟹ 逐位一致(Phase One 两张真样张输出哈希对拍过)。
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(static)
#endif
    for (int xi = 1; xi < int(wide); xi++)
    {
      float m0[4], d0[4], m1[4], d1[4], lmult[4];
      unsigned lc, lcol, lrow;
      for (lc = 0; lc < (unsigned)nc; lc += 2)
      {
        m0[lc] = mrow[lc * wide + xi - 1];
        d0[lc] = mrow[(lc + 1) * wide + xi - 1];
        m1[lc] = mrow[lc * wide + xi];
        d1[lc] = mrow[(lc + 1) * wide + xi];
      }
      const unsigned lcend = head[0] + xi * head[4];
      for (lrow = rstart; lrow < rstop; lrow++)
      {
        for (lc = 0; lc < (unsigned)nc; lc += 2)
        {
          lmult[lc] = m0[lc];
          lmult[lc + 1] = (m1[lc] - lmult[lc]) / head[4];
        }
        for (lcol = lcend - head[4];
             lcol < raw_width && lcol < lcend && lcol < unsigned(head[0] + head[2] - head[4]);
             lcol++)
        {
          lc = nc > 2 ? FC(lrow - top_margin, lcol - left_margin) : 0;
          if (!(lc & 1))
          {
            lc = unsigned(RAW(lrow, lcol) * lmult[lc]);
            RAW(lrow, lcol) = LIM(lc, 0, 65535);
          }
          for (lc = 0; lc < (unsigned)nc; lc += 2)
            lmult[lc] += lmult[lc + 1];
        }
        for (lc = 0; lc < (unsigned)nc; lc += 2)
        {
          m0[lc] += d0[lc];
          m1[lc] += d1[lc];
        }
      }
    }
    for (row = rstart; row < rstop; row++)
      for (x = 0; x < wide; x++)
        for (c = 0; c < (unsigned)nc; c += 2)
          mrow[c * wide + x] += mrow[(c + 1) * wide + x];
  }
  free(mrow);
}

int LibRaw::phase_one_correct()
{
  unsigned entries, tag, data, col, row, type;
  INT64 save;
  int len, i, j, k, cip, sum;
#if 0
  int val[4], dev[4], max;
#endif
  int head[9], diff, mindiff = INT_MAX;
  INT64 off_412 = 0;
  /* static */ const signed char dir[12][2] = {
      {-1, -1}, {-1, 1}, {1, -1},  {1, 1},  {-2, 0}, {0, -2},
      {0, 2},   {2, 0},  {-2, -2}, {-2, 2}, {2, -2}, {2, 2}};
  float poly[8], num, cfrac, frac, mult[2], *yval[2] = {NULL, NULL};
  ushort *xval[2];
  int qmult_applied = 0, qlin_applied = 0;
  std::vector<unsigned> badCols;

  if (!meta_length)
    return 0;
  fseek(ifp, meta_offset, SEEK_SET);
  order = get2();
  fseek(ifp, 6, SEEK_CUR);
  fseek(ifp, meta_offset + get4(), SEEK_SET);
  entries = get4();
  get4();
  INT64 fsize = ifp->size();

  try
  {
    while (entries--)
    {
      checkCancel();
      tag = get4();
      len = get4();
      data = get4();
      save = ftell(ifp);
      fseek(ifp, meta_offset + data, SEEK_SET);
#if 1
	  if (ifp->eof())
	  {
		  // skip bad or unknown tag
		  fseek(ifp, save, SEEK_SET);
		  continue;
	  }
#endif
      INT64 savepos = ftell(ifp);
	  if (len < 0 || (len > 8 && savepos + (INT64)len > 2 * fsize))
	  {
        fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
        continue;
	  }
      if (tag == 0x0400)
      { /* Sensor defects */
        while ((len -= 8) >= 0)
        {
          col = get2();
          row = get2();
          type = get2();
          get2();
          if (col >= raw_width)
            continue;
          if (type == 131 || type == 137) /* Bad column */
#if 0
            // Original code by Dave Coffin - it works better by
            // not employing special logic for G1 channel below.
            // Alternatively this column remap (including G1 channel
            // logic) should be called prior to black subtraction
            // unlike other corrections
            for (row = 0; row < raw_height; row++)
            {
              if (FC(row - top_margin, col - left_margin)==1)
              {
                for (sum = i = 0; i < 4; i++)
                  sum += val[i] = p1raw(row + dir[i][0], col + dir[i][1]);
                for (max = i = 0; i < 4; i++)
                {
                  dev[i] = abs((val[i] << 2) - sum);
                  if (dev[max] < dev[i])
                    max = i;
                }
                RAW(row, col) = (sum - val[max]) / 3.0 + 0.5;
              }
              else
              {
                for (sum = 0, i = 8; i < 12; i++)
                  sum += p1raw(row + dir[i][0], col + dir[i][1]);
                RAW(row, col) =
                  0.5 + sum * 0.0732233 +
                  (p1raw(row, col - 2) + p1raw(row, col + 2)) * 0.3535534;
              }
            }
#else
            // accumulae bad columns to be sorted later
            badCols.push_back(col);
#endif
          else if (type == 129)
          { /* Bad pixel */
            if (row >= raw_height)
              continue;
            j = (FC(row - top_margin, col - left_margin) != 1) * 4;
            unsigned count = 0;
            for (sum = 0, i = j; i < j + 8; i++)
              sum += p1rawc(row + dir[i][0], col + dir[i][1], count);
            if (count)
              RAW(row, col) = (sum + (count >> 1)) / count;
          }
        }
      }
      else if (tag == 0x0419)
      { /* Polynomial curve - output calibraion */
        for (get4(), i = 0; i < 8; i++)
          poly[i] = getrealf(LIBRAW_EXIFTAG_TYPE_FLOAT);
        poly[3] += (ph1.tag_210 - poly[7]) * poly[6] + 1;
        for (i = 0; i < 0x10000; i++)
        {
          num = (poly[5] * i + poly[3]) * i + poly[1];
          curve[i] = ushort(LIM(num, 0, 65535));
        }
        goto apply; /* apply to right half */
      }
      else if (tag == 0x041a)
      { /* Polynomial curve */
        for (i = 0; i < 4; i++)
          poly[i] = getrealf(LIBRAW_EXIFTAG_TYPE_FLOAT);
        for (i = 0; i < 0x10000; i++)
        {
          for (num = 0, j = 4; j--;)
            num = num * i + poly[j];
          curve[i] = ushort(LIM(num + i, 0, 65535));
        }
      apply: /* apply to whole image */
        for (row = 0; row < raw_height; row++)
        {
          checkCancel();
          for (col = (tag & 1) * ph1.split_col; col < raw_width; col++)
            RAW(row, col) = curve[RAW(row, col)];
        }
      }
      else if (tag == 0x0401)
      { /* All-color flat fields - luma calibration*/
        phase_one_flat_field(1, 2);
      }
      else if (tag == 0x0416 || tag == 0x0410)
      {
        // 0x410 - luma calibration
        phase_one_flat_field(0, 2);
      }
      else if (tag == 0x040b)
      { /* Red+blue flat field - croma calibration */
        phase_one_flat_field(0, 4);
      }
      else if (tag == 0x0412)
      {
        fseek(ifp, 36, SEEK_CUR);
        diff = abs(get2() - ph1.tag_21a);
        if (mindiff > diff)
        {
          mindiff = diff;
          off_412 = ftell(ifp) - 38;
        }
      }
      else if (tag == 0x041f && !qlin_applied && ph1.split_col > 0 && ph1.split_col < raw_width
		&& ph1.split_row > 0 && ph1.split_row < raw_height)
      { /* Quadrant linearization */
        ushort lc[2][2][16], ref[16];
        int qr, qc;
		bool baddiv = false;
        for (qr = 0; qr < 2; qr++)
			for (qc = 0; qc < 2; qc++)
			{
				for (i = 0; i < 16; i++)
					lc[qr][qc][i] = get4();
				if (lc[qr][qc][15] == 0)
					baddiv = true;
			}
		if(baddiv)
			continue;
        for (i = 0; i < 16; i++)
        {
          int v = 0;
          for (qr = 0; qr < 2; qr++)
            for (qc = 0; qc < 2; qc++)
              v += lc[qr][qc][i];
          ref[i] = (v + 2) >> 2;
        }
        for (qr = 0; qr < 2; qr++)
        {
          for (qc = 0; qc < 2; qc++)
          {
            int cx[19], cf[19];
            for (i = 0; i < 16; i++)
            {
              cx[1 + i] = lc[qr][qc][i];
              cf[1 + i] = ref[i];
            }
            cx[0] = cf[0] = 0;
            cx[17] = cf[17] = ((unsigned int)ref[15] * 65535) / lc[qr][qc][15];
            cf[18] = cx[18] = 65535;
            cubic_spline(cx, cf, 19);

            for (row = (qr ? ph1.split_row : 0);
                 row < unsigned(qr ? raw_height : ph1.split_row); row++)
            {
              checkCancel();
              for (col = (qc ? ph1.split_col : 0);
                   col < unsigned(qc ? raw_width : ph1.split_col); col++)
                RAW(row, col) = curve[RAW(row, col)];
            }
          }
        }
        qlin_applied = 1;
      }
      else if (tag == 0x041e && !qmult_applied)
      { /* Quadrant multipliers - output calibraion */
        float qmult[2][2] = {{1, 1}, {1, 1}};
        get4();
        get4();
        get4();
        get4();
        qmult[0][0] = 1.0f + getrealf(LIBRAW_EXIFTAG_TYPE_FLOAT);
        get4();
        get4();
        get4();
        get4();
        get4();
        qmult[0][1] = 1.0f + getrealf(LIBRAW_EXIFTAG_TYPE_FLOAT);
        get4();
        get4();
        get4();
        qmult[1][0] = 1.0f + getrealf(LIBRAW_EXIFTAG_TYPE_FLOAT);
        get4();
        get4();
        get4();
        qmult[1][1] = 1.0f + getrealf(LIBRAW_EXIFTAG_TYPE_FLOAT);
        for (row = 0; row < raw_height; row++)
        {
          checkCancel();
          for (col = 0; col < raw_width; col++)
          {
            i = int(qmult[row >= (unsigned)ph1.split_row][col >= (unsigned)ph1.split_col] *
                RAW(row, col));
            RAW(row, col) = LIM(i, 0, 65535);
          }
        }
        qmult_applied = 1;
      }
      else if (tag == 0x0431 && !qmult_applied && ph1.split_col > 0 && ph1.split_col < raw_width 
		&& ph1.split_row > 0 && ph1.split_row < raw_height)
      { /* Quadrant combined - four tile gain calibration */
        ushort lc[2][2][7], ref[7];
        int qr, qc;
        for (i = 0; i < 7; i++)
          ref[i] = get4();
        for (qr = 0; qr < 2; qr++)
          for (qc = 0; qc < 2; qc++)
            for (i = 0; i < 7; i++)
              lc[qr][qc][i] = get4();
        for (qr = 0; qr < 2; qr++)
        {
          for (qc = 0; qc < 2; qc++)
          {
            int cx[9], cf[9];
            for (i = 0; i < 7; i++)
            {
              cx[1 + i] = ref[i];
              cf[1 + i] = ((unsigned)ref[i] * lc[qr][qc][i]) / 10000;
            }
            cx[0] = cf[0] = 0;
            cx[8] = cf[8] = 65535;
            cubic_spline(cx, cf, 9);
            for (row = (qr ? ph1.split_row : 0);
                 row < unsigned(qr ? raw_height : ph1.split_row); row++)
            {
              checkCancel();
              for (col = (qc ? ph1.split_col : 0);
                   col < unsigned(qc ? raw_width : ph1.split_col); col++)
                RAW(row, col) = curve[RAW(row, col)];
            }
          }
        }
        qmult_applied = 1;
        qlin_applied = 1;
      }
      fseek(ifp, save, SEEK_SET);
    }
    if (!badCols.empty())
    {
      qsort(badCols.data(), badCols.size(), sizeof(unsigned), unsigned_cmp);
      bool prevIsolated = true;
      for (i = 0; i < (int)badCols.size(); ++i)
      {
        bool nextIsolated = i == ((int)(badCols.size()-1)) || badCols[i+1]>badCols[i]+4;
        for (row = 0; row < raw_height; ++row)
          if (prevIsolated && nextIsolated)
            phase_one_fix_pixel_grad(row,badCols[i]);
          else
            phase_one_fix_col_pixel_avg(row,badCols[i]);
        prevIsolated = nextIsolated;
      }
    }
    if (off_412)
    {
      fseek(ifp, off_412, SEEK_SET);
      for (i = 0; i < 9; i++)
        head[i] = get4() & 0x7fff;
	  unsigned w0 = head[1] * head[3], w1 = head[2] * head[4];
	  if (w0 > 10240000 || w1 > 10240000)
		  throw LIBRAW_EXCEPTION_ALLOC;
	  if (w0 < 1 || w1 < 1)
		  throw LIBRAW_EXCEPTION_IO_CORRUPT;
      yval[0] = (float *)calloc(head[1] * head[3] + head[2] * head[4], 6);
      yval[1] = (float *)(yval[0] + head[1] * head[3]);
      xval[0] = (ushort *)(yval[1] + head[2] * head[4]);
      xval[1] = (ushort *)(xval[0] + head[1] * head[3]);
      get2();
      for (i = 0; i < 2; i++)
        for (j = 0; j < head[i + 1] * head[i + 3]; j++)
          yval[i][j] = getrealf(LIBRAW_EXIFTAG_TYPE_FLOAT);
      for (i = 0; i < 2; i++)
        for (j = 0; j < head[i + 1] * head[i + 3]; j++)
          xval[i][j] = get2();
      for (row = 0; row < raw_height; row++)
      {
        checkCancel();
        for (col = 0; col < raw_width; col++)
        {
          cfrac = (float)col * head[3] / raw_width;
		  cip = (int)cfrac;
          cfrac -= cip;
          num = RAW(row, col) * 0.5f;
          for (i = cip; i < cip + 2 && i < head[3]; i++)
          {
            for (k = j = 0; j < head[1]; j++)
              if (num < xval[0][k = head[1] * i + j])
                break;
			if (j == 0 || j == head[1] || k < 1 || k >= w0+w1)
				frac = 0;
			else
			{
				int xdiv = (xval[0][k] - xval[0][k - 1]);
				frac = xdiv ? (xval[0][k] - num) / (xval[0][k] - xval[0][k - 1]) : 0;
			}
			if (k < w0 + w1)
				mult[i - cip] = yval[0][k > 0 ? k - 1 : 0] * frac + yval[0][k] * (1 - frac);
			else
				mult[i - cip] = 0;
          }
          i = int(((mult[0] * (1.f - cfrac) + mult[1] * cfrac) * row + num) * 2.f);
          RAW(row, col) = LIM(i, 0, 65535);
        }
      }
      free(yval[0]);
    }
  }
  catch (...)
  {
    if (yval[0])
      free(yval[0]);
    return LIBRAW_CANCELLED_BY_CALLBACK;
  }
  return 0;
}

void LibRaw::phase_one_load_raw()
{
  int a, b, i;
  ushort akey, bkey, t_mask;

  fseek(ifp, ph1.key_off, SEEK_SET);
  akey = get2();
  bkey = get2();
  t_mask = ph1.format == 1 ? 0x5555 : 0x1354;
  if (ph1.black_col || ph1.black_row)
  {
    imgdata.rawdata.ph1_cblack =
        (short(*)[2])calloc(raw_height * 2, sizeof(ushort));
    imgdata.rawdata.ph1_rblack =
        (short(*)[2])calloc(raw_width * 2, sizeof(ushort));
    if (ph1.black_col)
    {
      fseek(ifp, ph1.black_col, SEEK_SET);
      read_shorts((ushort *)imgdata.rawdata.ph1_cblack[0], raw_height * 2);
    }
    if (ph1.black_row)
    {
      fseek(ifp, ph1.black_row, SEEK_SET);
      read_shorts((ushort *)imgdata.rawdata.ph1_rblack[0], raw_width * 2);
    }
  }
  fseek(ifp, data_offset, SEEK_SET);
  read_shorts(raw_image, raw_width * raw_height);
  if (ph1.format)
    for (i = 0; i < raw_width * raw_height; i += 2)
    {
      a = raw_image[i + 0] ^ akey;
      b = raw_image[i + 1] ^ bkey;
      raw_image[i + 0] = (a & t_mask) | (b & ~t_mask);
      raw_image[i + 1] = (b & t_mask) | (a & ~t_mask);
    }
}

unsigned LibRaw::ph1_bithuff(int nbits, ushort *huff)
{
#ifndef LIBRAW_NOTHREADS
#define bitbuf tls->ph1_bits.bitbuf
#define vbits tls->ph1_bits.vbits
#else
  static UINT64 bitbuf = 0;
  static int vbits = 0;
#endif
  unsigned c;

  if (nbits == -1)
  {
	  bitbuf = vbits = 0;
	  return 0;
  }
  if (nbits == 0)
    return 0;
  if (vbits < nbits)
  {
    bitbuf = bitbuf << 32 | get4();
    vbits += 32;
  }
  c = unsigned ((bitbuf << (64 - vbits) >> (64 - nbits)) & 0xffffffff);
  if (huff)
  {
    vbits -= huff[c] >> 8;
    return (uchar)huff[c];
  }
  vbits -= nbits;
  return c;
#ifndef LIBRAW_NOTHREADS
#undef bitbuf
#undef vbits
#endif
}

// ALPA patch(IIQ 并行解压,2026-10-24):见 phase_one_load_raw_c 里的调用处。成功 ⟹ true(raw_image 已填好);否则 false,调用方走原串行路径。
bool LibRaw::alpa_phase_one_load_raw_c_parallel(const int *offset, const int *length)
{
  INT64 bytes = 0;
  const unsigned char *mem = libraw_internal_data.internal_data.input->alpa_memory(&bytes);
  if (!mem || bytes <= 0 || !raw_image)
    return false;
  const INT64 base = INT64(data_offset);
  const short ord = order;
  const int fmt = ph1.format;
  const int rw = raw_width, rh = raw_height;
  ushort *raw = raw_image;
  const ushort *crv = curve;
  // 原版 len 是函数级变量、跨行延续(复读 r1 P1)。按行解一行:inLen / inKnown = 行首的 len 及其是否已知,outLen = 行末的 len。
  // 返回 0 = 成功;1 = 越界 / 校验失败(整段交回原串行路径);2 = 行首遇到「长度不变」而 len 不可知(留到第二遍按行序带着上一行的 len 补解)。
  enum { ROW_OK = 0, ROW_BAD = 1, ROW_NEEDS_CARRY = 2 };
  auto decodeRow = [&](int row, const int inLen[2], bool inKnown, int outLen[2]) -> int {
    INT64 pos = base + offset[row];
    UINT64 bitbuf = 0;
    int vbits = 0, rowbad = 0;
    auto bits = [&](int n) -> unsigned {
      if (n == 0)
        return 0;
      if (vbits < n)
      {
        uchar s4[4] = {0xff, 0xff, 0xff, 0xff};
        if (pos < 0 || pos + 4 > bytes)
          rowbad = 1; // 原版此处读到 EOF:交给串行路径按原语义处理
        else
          memcpy(s4, mem + pos, 4);
        pos += 4;
        bitbuf = bitbuf << 32 | libraw_sget4_static(ord, s4);
        vbits += 32;
      }
      unsigned c = unsigned((bitbuf << (64 - vbits) >> (64 - n)) & 0xffffffff);
      vbits -= n;
      return c;
    };
    int len[2] = {inLen[0], inLen[1]}, pred[2] = {0, 0}, i, j;
    bool known[2] = {inKnown, inKnown};
    ushort *dst = raw + size_t(row) * rw;
    for (int col = 0; col < rw; col++)
    {
      if (col >= (rw & -8))
        len[0] = len[1] = 14;
      else if ((col & 7) == 0)
        for (i = 0; i < 2; i++)
        {
          for (j = 0; j < 5 && !bits(1); j++)
            ;
          if (j--)
          {
            len[i] = length[j * 2 + bits(1)];
            known[i] = true;
          }
          else if (!known[i])
            return ROW_NEEDS_CARRY;
        }
      if (rowbad)
        return ROW_BAD;
      ushort px;
      if ((i = len[col & 1]) == 14)
        px = ushort(pred[col & 1] = bits(16));
      else
        px = ushort(pred[col & 1] += bits(i) + 1 - (1 << (i - 1)));
      if (rowbad || (pred[col & 1] >> 16))
        return ROW_BAD;
      if (fmt == 5 && px < 256)
        px = crv[px];
      dst[col] = fmt == 8 ? px : ushort(px << 2);
    }
    outLen[0] = len[0];
    outLen[1] = len[1];
    return ROW_OK;
  };

  // 宽度不是 8 的倍数:上一行末尾必定落在 col >= rw & -8 那段,len 恒为 {14,14} ⟹ 第 1 行起行首 len 已知。
  // 第 0 行在原版里是未初始化值:那里遇到「长度不变」就整段交回原串行路径(行为与原版一致)。
  const bool tailFixed = (rw & 7) != 0;
  const int fixedLen[2] = {14, 14};
  std::vector<int> endLen(size_t(rh) * 2, 0);
  std::vector<unsigned char> status(rh, ROW_OK);
  int bad = 0;
  const int CHUNK = 64;
  for (int r0 = 0; r0 < rh && !bad; r0 += CHUNK)
  {
    checkCancel();
    const int r1 = MIN(rh, r0 + CHUNK);
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(dynamic, 1) reduction(| : bad)
#endif
    for (int row = r0; row < r1; row++)
    {
      const int st = decodeRow(row, fixedLen, row > 0 && tailFixed, &endLen[size_t(row) * 2]);
      status[row] = (unsigned char)st;
      if (st == ROW_BAD || (st == ROW_NEEDS_CARRY && row == 0))
        bad = 1;
    }
  }
  if (bad)
    return false;
  // 第二遍(复读 r2 P3):只补解行首依赖上一行 len 的那些行,按行序串行,上一行的行末 len 此时已确定;其余行的并行结果保留。
  for (int row = 1; row < rh; row++)
  {
    if (status[row] != ROW_NEEDS_CARRY)
      continue;
    if ((row & 63) == 0)
      checkCancel();
    if (decodeRow(row, &endLen[size_t(row - 1) * 2], true, &endLen[size_t(row) * 2]) != ROW_OK)
      return false;
  }
  return true;
}

void LibRaw::phase_one_load_raw_c()
{
  static const int length[] = {8, 7, 6, 9, 11, 10, 5, 12, 14, 13};
  int *offset, len[2], pred[2], row, col, i, j;
  ushort *pixel;
  short(*c_black)[2], (*r_black)[2];
  if (ph1.format == 6)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  pixel = (ushort *)calloc(raw_width * 3 + raw_height * 4, 2);
  offset = (int *)(pixel + raw_width);
  fseek(ifp, strip_offset, SEEK_SET);
  for (row = 0; row < raw_height; row++)
    offset[row] = get4();
  c_black = (short(*)[2])(offset + raw_height);
  fseek(ifp, ph1.black_col, SEEK_SET);
  if (ph1.black_col)
    read_shorts((ushort *)c_black[0], raw_height * 2);
  r_black = c_black + raw_height;
  fseek(ifp, ph1.black_row, SEEK_SET);
  if (ph1.black_row)
    read_shorts((ushort *)r_black[0], raw_width * 2);

  // Copy data to internal copy (ever if not read)
  if (ph1.black_col || ph1.black_row)
  {
    imgdata.rawdata.ph1_cblack =
        (short(*)[2])calloc(raw_height * 2, sizeof(ushort));
    memmove(imgdata.rawdata.ph1_cblack, (ushort *)c_black[0],
            raw_height * 2 * sizeof(ushort));
    imgdata.rawdata.ph1_rblack =
        (short(*)[2])calloc(raw_width * 2, sizeof(ushort));
    memmove(imgdata.rawdata.ph1_rblack, (ushort *)r_black[0],
            raw_width * 2 * sizeof(ushort));
  }

  for (i = 0; i < 256; i++)
    curve[i] = ushort(float(i * i) / 3.969f + 0.5f);
  // ALPA patch(IIQ 并行解压,2026-10-24):每行有独立偏移 ⟹ 数据在内存里时按行并行直读(位读取器线程局部,取数同 ph1_bithuff / get4)。
  // 任一行越出数据或校验失败(pred 溢出)⟹ 整段丢弃并行结果、退回下面的原串行循环,保证合法文件逐位一致、坏文件行为与原版相同。
  try
  {
    // 在 try 里调:并行段的 checkCancel 抛出时同样走下面的 catch 释放 pixel(复读 r1 P2)
    if (alpa_phase_one_load_raw_c_parallel(offset, length))
    {
      free(pixel);
      maximum = 0xfffc - ph1.t_black;
      return;
    }
    for (row = 0; row < raw_height; row++)
    {
      checkCancel();
      fseek(ifp, data_offset + offset[row], SEEK_SET);
      ph1_bits(-1);
      pred[0] = pred[1] = 0;
      for (col = 0; col < raw_width; col++)
      {
        if (col >= (raw_width & -8))
          len[0] = len[1] = 14;
        else if ((col & 7) == 0)
          for (i = 0; i < 2; i++)
          {
            for (j = 0; j < 5 && !ph1_bits(1); j++)
              ;
            if (j--)
              len[i] = length[j * 2 + ph1_bits(1)];
          }
        if ((i = len[col & 1]) == 14)
          pixel[col] = pred[col & 1] = ph1_bits(16);
        else
          pixel[col] = pred[col & 1] += ph1_bits(i) + 1 - (1 << (i - 1));
        if (pred[col & 1] >> 16)
          derror();
        if (ph1.format == 5 && pixel[col] < 256)
          pixel[col] = curve[pixel[col]];
      }
      if (ph1.format == 8)
        memmove(&RAW(row, 0), &pixel[0], raw_width * 2);
      else
        for (col = 0; col < raw_width; col++)
          RAW(row, col) = pixel[col] << 2;
    }
  }
  catch (...)
  {
    free(pixel);
    throw;
  }
  free(pixel);
  maximum = 0xfffc - ph1.t_black;
}

void LibRaw::hasselblad_load_raw()
{
  struct jhead jh;
  int shot, row, col, *back[5]={0,0,0,0,0},
 	 len[2], diff[12], pred, sh, f, c;
  unsigned s;
  unsigned upix, urow, ucol;
  ushort *ip;

  if (!ljpeg_start(&jh, 0))
    return;
  order = 0x4949;
  ph1_bits(-1);
  try
  {
    back[4] = (int *)calloc(raw_width, 3 * sizeof **back);
    FORC3 back[c] = back[4] + c * raw_width;
    cblack[6] >>= sh = tiff_samples > 1;
    shot = LIM(shot_select, 1, tiff_samples) - 1;
    for (row = 0; row < raw_height; row++)
    {
      checkCancel();
      FORC4 back[(c + 3) & 3] = back[c];
      for (col = 0; col < raw_width; col += 2)
      {
        for (s = 0; s < tiff_samples * 2; s += 2)
        {
          FORC(2) len[c] = ph1_huff(jh.huff[0]);
          FORC(2)
          {
            diff[s + c] = ph1_bits(len[c]);
            if (len[c] > 0 && (diff[s + c] & (1 << (len[c] - 1))) == 0)
              diff[s + c] -= (1 << len[c]) - 1;
            if (diff[s + c] == 65535)
              diff[s + c] = -32768;
          }
        }
        for (s = col; s < unsigned(col + 2); s++)
        {
          pred = 0x8000 + load_flags;
          if (col)
            pred = back[2][s - 2];
          if (col && row > 1)
            switch (jh.psv)
            {
            case 11:
              pred += back[0][s] / 2 - back[0][s - 2] / 2;
              break;
            }
          f = (row & 1) * 3 ^ ((col + s) & 1);
          FORC(int(tiff_samples))
          {
            pred += diff[(s & 1) * tiff_samples + c];
            upix = pred >> sh & 0xffff;
            if (raw_image && c == shot)
              RAW(row, s) = upix;
            if (image)
            {
              urow = row - top_margin + (c & 1);
              ucol = col - left_margin - ((c >> 1) & 1);
              ip = &image[urow * width + ucol][f];
              if (urow < height && ucol < width)
                *ip = c < 4 ? upix : (*ip + upix) >> 1;
            }
          }
          back[2][s] = pred;
        }
      }
    }
  }
  catch (...)
  {
    if(back[4])
    	free(back[4]);
    ljpeg_end(&jh);
    throw;
  }
  if(back[4])
    free(back[4]);
  ljpeg_end(&jh);
  if (image)
    mix_green = 1;
}

void LibRaw::leaf_hdr_load_raw()
{
  ushort *pixel = 0;
  unsigned tile = 0, r, c, row, col;

  if (!filters || !raw_image)
  {
    if (!image)
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
    pixel = (ushort *)calloc(raw_width, sizeof *pixel);
  }
  try
  {
    FORC(tiff_samples)
    for (r = 0; r < raw_height; r++)
    {
      checkCancel();
      if (r % tile_length == 0)
      {
        fseek(ifp, data_offset + 4 * tile++, SEEK_SET);
        fseek(ifp, get4(), SEEK_SET);
      }
      if (filters && c != shot_select)
        continue;
      if (filters && raw_image)
        pixel = raw_image + r * raw_width;
      read_shorts(pixel, raw_width);
      if (!filters && image && (row = r - top_margin) < height)
        for (col = 0; col < width && col + left_margin < raw_width; col++)
          image[row * width + col][c] = pixel[col + left_margin];
    }
  }
  catch (...)
  {
    if (!filters)
      free(pixel);
    throw;
  }
  if (!filters)
  {
    maximum = 0xffff;
    raw_color = 1;
    free(pixel);
  }
}

void LibRaw::unpacked_load_raw_FujiDBP()
/*
for Fuji DBP for GX680, aka DX-2000
  DBP_tile_width = 688;
  DBP_tile_height = 3856;
  DBP_n_tiles = 8;
*/
{
  int scan_line, tile_n;
  int nTiles;

  nTiles = 8;
  tile_width = raw_width / nTiles;

  ushort *tile;
  tile = (ushort *)calloc(raw_height, tile_width * 2);

  for (tile_n = 0; tile_n < nTiles; tile_n++)
  {
    read_shorts(tile, tile_width * raw_height);
    for (scan_line = 0; scan_line < raw_height; scan_line++)
    {
      memcpy(&raw_image[scan_line * raw_width + tile_n * tile_width],
             &tile[scan_line * tile_width], tile_width * 2);
    }
  }
  free(tile);
  fseek(ifp, -2, SEEK_CUR); // avoid EOF error
}

void LibRaw::sinar_4shot_load_raw()
{
  ushort *pixel;
  unsigned shot, row, col, r, c;

  if (raw_image)
  {
    shot = LIM(shot_select, 1, 4) - 1;
    fseek(ifp, data_offset + shot * 4, SEEK_SET);
    fseek(ifp, get4(), SEEK_SET);
    unpacked_load_raw();
    return;
  }
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
  pixel = (ushort *)calloc(raw_width, sizeof *pixel);
  try
  {
    for (shot = 0; shot < 4; shot++)
    {
      checkCancel();
      fseek(ifp, data_offset + shot * 4, SEEK_SET);
      fseek(ifp, get4(), SEEK_SET);
      for (row = 0; row < raw_height; row++)
      {
        read_shorts(pixel, raw_width);
        if ((r = row - top_margin - (shot >> 1 & 1)) >= height)
          continue;
        for (col = 0; col < raw_width; col++)
        {
          if ((c = col - left_margin - (shot & 1)) >= width)
            continue;
          image[r * width + c][(row & 1) * 3 ^ (~col & 1)] = pixel[col];
        }
      }
    }
  }
  catch (...)
  {
    free(pixel);
    throw;
  }
  free(pixel);
  mix_green = 1;
}

void LibRaw::imacon_full_load_raw()
{
  if (!image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
  int row, col;

  unsigned short *buf =
      (unsigned short *)malloc(width * 3 * sizeof(unsigned short));

  for (row = 0; row < height; row++)
  {
    checkCancel();
    read_shorts(buf, width * 3);
    unsigned short(*rowp)[4] = &image[row * width];
    for (col = 0; col < width; col++)
    {
      rowp[col][0] = buf[col * 3];
      rowp[col][1] = buf[col * 3 + 1];
      rowp[col][2] = buf[col * 3 + 2];
      rowp[col][3] = 0;
    }
  }
  free(buf);
}
