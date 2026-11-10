/* -*- C++ -*-
 * Copyright 2019-2026 LibRaw LLC (info@libraw.org)
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

inline unsigned int __DNG_HalfToFloat(ushort halfValue)
{
  int sign = (halfValue >> 15) & 0x00000001;
  int exponent = (halfValue >> 10) & 0x0000001f;
  int mantissa = halfValue & 0x000003ff;
  if (exponent == 0)
  {
    if (mantissa == 0)
    {
      return (unsigned int)(sign << 31);
    }
    else
    {
      while (!(mantissa & 0x00000400))
      {
        mantissa <<= 1;
        exponent -= 1;
      }
      exponent += 1;
      mantissa &= ~0x00000400;
    }
  }
  else if (exponent == 31)
  {
    if (mantissa == 0)
    {
      return (unsigned int)((sign << 31) | ((0x1eL + 127 - 15) << 23) |
                            (0x3ffL << 13));
    }
    else
    {
      return 0;
    }
  }
  exponent += (127 - 15);
  mantissa <<= 13;
  return (unsigned int)((sign << 31) | (exponent << 23) | mantissa);
}

inline unsigned int __DNG_FP24ToFloat(const unsigned char *input)
{
  int sign = (input[0] >> 7) & 0x01;
  int exponent = (input[0]) & 0x7F;
  int mantissa = (((int)input[1]) << 8) | input[2];
  if (exponent == 0)
  {
    if (mantissa == 0)
    {
      return (unsigned int)(sign << 31);
    }
    else
    {
      while (!(mantissa & 0x00010000))
      {
        mantissa <<= 1;
        exponent -= 1;
      }
      exponent += 1;
      mantissa &= ~0x00010000;
    }
  }
  else if (exponent == 127)
  {
    if (mantissa == 0)
    {
      return (unsigned int)((sign << 31) | ((0x7eL + 128 - 64) << 23) |
                            (0xffffL << 7));
    }
    else
    {
      // Nan -- Just set to zero.
      return 0;
    }
  }
  exponent += (128 - 64);
  mantissa <<= 7;
  return (uint32_t)((sign << 31) | (exponent << 23) | mantissa);
}

inline void DecodeDeltaBytes(unsigned char *bytePtr, int cols, int channels)
{
  if (channels == 1)
  {
    unsigned char b0 = bytePtr[0];
    bytePtr += 1;
    for (int col = 1; col < cols; ++col)
    {
      b0 += bytePtr[0];
      bytePtr[0] = b0;
      bytePtr += 1;
    }
  }
  else if (channels == 3)
  {
    unsigned char b0 = bytePtr[0];
    unsigned char b1 = bytePtr[1];
    unsigned char b2 = bytePtr[2];
    bytePtr += 3;
    for (int col = 1; col < cols; ++col)
    {
      b0 += bytePtr[0];
      b1 += bytePtr[1];
      b2 += bytePtr[2];
      bytePtr[0] = b0;
      bytePtr[1] = b1;
      bytePtr[2] = b2;
      bytePtr += 3;
    }
  }
  else if (channels == 4)
  {
    unsigned char b0 = bytePtr[0];
    unsigned char b1 = bytePtr[1];
    unsigned char b2 = bytePtr[2];
    unsigned char b3 = bytePtr[3];
    bytePtr += 4;
    for (int col = 1; col < cols; ++col)
    {
      b0 += bytePtr[0];
      b1 += bytePtr[1];
      b2 += bytePtr[2];
      b3 += bytePtr[3];
      bytePtr[0] = b0;
      bytePtr[1] = b1;
      bytePtr[2] = b2;
      bytePtr[3] = b3;
      bytePtr += 4;
    }
  }
  else
  {
    for (int col = 1; col < cols; ++col)
    {
      for (int chan = 0; chan < channels; ++chan)
      {
        bytePtr[chan + channels] += bytePtr[chan];
      }
      bytePtr += channels;
    }
  }
}

#ifdef USE_ZLIB
static void DecodeFPDelta(unsigned char *input, unsigned char *output, int cols,
                          int channels, int bytesPerSample)
{
  DecodeDeltaBytes(input, cols * bytesPerSample, channels);
  int32_t rowIncrement = cols * channels;

  if (bytesPerSample == 2)
  {

#if LibRawBigEndian
    const unsigned char *input0 = input;
    const unsigned char *input1 = input + rowIncrement;
#else
    const unsigned char *input1 = input;
    const unsigned char *input0 = input + rowIncrement;
#endif
    for (int col = 0; col < rowIncrement; ++col)
    {
      output[0] = input0[col];
      output[1] = input1[col];
      output += 2;
    }
  }
  else if (bytesPerSample == 3)
  {
    const unsigned char *input0 = input;
    const unsigned char *input1 = input + rowIncrement;
    const unsigned char *input2 = input + rowIncrement * 2;
    for (int col = 0; col < rowIncrement; ++col)
    {
      output[0] = input0[col];
      output[1] = input1[col];
      output[2] = input2[col];
      output += 3;
    }
  }
  else
  {
#if LibRawBigEndian
    const unsigned char *input0 = input;
    const unsigned char *input1 = input + rowIncrement;
    const unsigned char *input2 = input + rowIncrement * 2;
    const unsigned char *input3 = input + rowIncrement * 3;
#else
    const unsigned char *input3 = input;
    const unsigned char *input2 = input + rowIncrement;
    const unsigned char *input1 = input + rowIncrement * 2;
    const unsigned char *input0 = input + rowIncrement * 3;
#endif
    for (int col = 0; col < rowIncrement; ++col)
    {
      output[0] = input0[col];
      output[1] = input1[col];
      output[2] = input2[col];
      output[3] = input3[col];
      output += 4;
    }
  }
}
#endif

static float expandFloats(unsigned char *dst, int tileWidth, int bytesps)
{
  float max = 0.f;
  if (bytesps == 2)
  {
    uint16_t *dst16 = (ushort *)dst;
    uint32_t *dst32 = (unsigned int *)dst;
    float *f32 = (float *)dst;
    for (int index = tileWidth - 1; index >= 0; --index)
    {
      dst32[index] = __DNG_HalfToFloat(dst16[index]);
      max = MAX(max, f32[index]);
    }
  }
  else if (bytesps == 3)
  {
    uint8_t *dst8 = ((unsigned char *)dst) + (tileWidth - 1) * 3;
    uint32_t *dst32 = (unsigned int *)dst;
    float *f32 = (float *)dst;
    for (int index = tileWidth - 1; index >= 0; --index, dst8 -= 3)
    {
      dst32[index] = __DNG_FP24ToFloat(dst8);
      max = MAX(max, f32[index]);
    }
  }
  else if (bytesps == 4)
  {
    float *f32 = (float *)dst;
    for (int index = 0; index < tileWidth; index++)
      max = MAX(max, f32[index]);
  }
  return max;
}

struct tile_stripe_data_t
{
    bool tiled, striped;
    int tileCnt;
    unsigned tileWidth, tileHeight, tilesH, tilesV;
    INT64 maxBytesInTile;
    std::vector<INT64> tOffsets, tBytes;
    tile_stripe_data_t() : tiled(false), striped(false),tileCnt(0),
        tileWidth(0),tileHeight(0),tilesH(0),tilesV(0),
        maxBytesInTile(0){}
    void init(tiff_ifd_t *ifd, const libraw_image_sizes_t&, const unpacker_data_t&,
        short _order,
        LibRaw_abstract_datastream *stream);
};

static unsigned static_get4(LibRaw_abstract_datastream *stream, short _order)
{
    uchar str[4] = { 0xff, 0xff, 0xff, 0xff };
    stream->read(str, 1, 4);
    return libraw_sget4_static(_order, str);
}


void tile_stripe_data_t::init(tiff_ifd_t *ifd, const libraw_image_sizes_t& sizes, 
    const unpacker_data_t& unpacker_data, short _order, LibRaw_abstract_datastream *stream)
{
    tiled = (unpacker_data.tile_width <= sizes.raw_width) && (unpacker_data.tile_length <= sizes.raw_height);
    striped = (ifd->rows_per_strip > 0 && ifd->rows_per_strip < sizes.raw_height) && ifd->strip_byte_counts_count > 0;

    tileWidth = tiled ? unpacker_data.tile_width : sizes.raw_width;
    tileHeight = tiled ? unpacker_data.tile_length :(striped ? ifd->rows_per_strip : sizes.raw_height);
    tilesH = tiled ? (sizes.raw_width + tileWidth - 1) / tileWidth : 1;
    tilesV = tiled ? (sizes.raw_height + tileHeight - 1) / tileHeight :
        (striped ? ((sizes.raw_height + ifd->rows_per_strip - 1) / ifd->rows_per_strip) : 1);
    tileCnt = tilesH * tilesV;

    if (tileCnt < 1 || tileCnt > 1000000)
        throw LIBRAW_EXCEPTION_DECODE_RAW;

    tOffsets = std::vector<INT64>(tileCnt,0);
    tBytes = std::vector <INT64>(tileCnt,0);

    if (tiled)
        for (int t = 0; t < tileCnt; ++t)
            tOffsets[t] = static_get4(stream, _order);
    else if (striped)
        for (int t = 0; t < tileCnt && t < ifd->strip_offsets_count; ++t)
            tOffsets[t] = ifd->strip_offsets[t];
    else
        tOffsets[0] = ifd->offset;

    maxBytesInTile = 0;

    if (tileCnt == 1 || (!tiled && !striped))
        tBytes[0] = maxBytesInTile = ifd->bytes;
    else if (tiled)
    {
        // ifd->bytes points to tile size table if more than 1 tile exists
        stream->seek(ifd->bytes, SEEK_SET);
        for (int t = 0; t < tileCnt; ++t)
        {
            tBytes[t] = static_get4(stream, _order); ;
            maxBytesInTile = MAX(maxBytesInTile, tBytes[t]);
        }
    }
    else if (striped)
        for (int t = 0; t < tileCnt && t < ifd->strip_byte_counts_count; ++t)
        {
            tBytes[t] = ifd->strip_byte_counts[t];
            maxBytesInTile = MAX(maxBytesInTile, tBytes[t]);
        }
}

#ifdef USE_ZLIB
void LibRaw::deflate_dng_load_raw()
{
  int iifd = find_ifd_by_offset(libraw_internal_data.unpacker_data.data_offset);
  if(iifd < 0 || iifd > (int)libraw_internal_data.identify_data.tiff_nifds)
      throw LIBRAW_EXCEPTION_DECODE_RAW;
  struct tiff_ifd_t *ifd = &tiff_ifd[iifd];

  float *float_raw_image = 0;
  float max = 0.f;

  if (ifd->samples != 1 && ifd->samples != 3 && ifd->samples != 4)
    throw LIBRAW_EXCEPTION_DECODE_RAW; 

  if (libraw_internal_data.unpacker_data.tiff_samples != (unsigned)ifd->samples)
    throw LIBRAW_EXCEPTION_DECODE_RAW; // Wrong IFD

  if (imgdata.idata.filters && ifd->samples > 1)
    throw LIBRAW_EXCEPTION_DECODE_RAW;

  int _bytesps = ifd->bps >> 3;
  if (_bytesps < 2 || _bytesps > 4)
    throw LIBRAW_EXCEPTION_DECODE_RAW; // Only 16, 24 and 32 bits are supported

  tile_stripe_data_t tiles;
  tiles.init(ifd, imgdata.sizes, libraw_internal_data.unpacker_data, libraw_internal_data.unpacker_data.order,
      libraw_internal_data.internal_data.input);

  if (tiles.tBytes.size() < 1)
	  throw LIBRAW_EXCEPTION_IO_CORRUPT;

  // Ensure less then 2GB per compressed tile
  INT64 maxcomprlen = tiles.tBytes[0];
  for (int i = 1; i < tiles.tBytes.size(); i++)
	  maxcomprlen = MAX(maxcomprlen, tiles.tBytes[i]);

  if(maxcomprlen >= (1LL << 31) || maxcomprlen < 0)
	  throw LIBRAW_EXCEPTION_TOOBIG;
  
  // Max bytes: 2^16 raw width * 2^2 bytes/pixel * 2^2 channels = 2^20, so check against 2^22
  INT64 rowbytes = INT64(MAX(tiles.tileWidth, imgdata.sizes.raw_width)) * 4ULL * INT64(ifd->samples);
  if (rowbytes > (1LL << 22))
    throw LIBRAW_EXCEPTION_TOOBIG;

  if (ifd->sample_format == 3)
  {
    INT64 raw_bytes = INT64(tiles.tileCnt) * INT64(tiles.tileWidth) * INT64(tiles.tileHeight) * INT64(ifd->samples) * sizeof(float);
    if (raw_bytes > INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024))
      throw LIBRAW_EXCEPTION_TOOBIG;
    float_raw_image = (float *)calloc(raw_bytes, 1);
  }
  else
    throw LIBRAW_EXCEPTION_DECODE_RAW; // Only float deflated supported

  int xFactor;
  switch (ifd->predictor)
  {
  case 3:
  default:
    xFactor = 1;
    break;
  case 34894:
    xFactor = 2;
    break;
  case 34895:
    xFactor = 4;
    break;
  }

  INT64 tilePixels =  INT64(tiles.tileWidth) * INT64(tiles.tileHeight);
  unsigned pixelSize = sizeof(float) * ifd->samples;
  INT64 tileBytes = tilePixels * INT64(pixelSize);
  INT64 tileRowBytes = INT64(tiles.tileWidth) * INT64(pixelSize);

  if(INT64(tiles.maxBytesInTile) > INT64(imgdata.rawparams.max_raw_memory_mb) * 1024LL * 1024LL )
    throw LIBRAW_EXCEPTION_TOOBIG;

  if (tileBytes + tileRowBytes > INT64(imgdata.rawparams.max_raw_memory_mb) * 1024LL * 1024LL)
    throw LIBRAW_EXCEPTION_TOOBIG;

  std::vector<uchar> cBuffer(tiles.maxBytesInTile,0);
  std::vector<uchar> uBuffer(tileBytes + tileRowBytes,0); // extra row for decoding

  for (size_t y = 0, t = 0; y < imgdata.sizes.raw_height; y += tiles.tileHeight)
    {
      for (size_t x = 0; x < imgdata.sizes.raw_width; x += tiles.tileWidth, ++t)
      {
		if(t >= tiles.tOffsets.size()) // should not happen but check anyway
          throw LIBRAW_EXCEPTION_IO_CORRUPT; 

        libraw_internal_data.internal_data.input->seek(tiles.tOffsets[t], SEEK_SET);
        int bytesread = libraw_internal_data.internal_data.input->read(cBuffer.data(), 1, tiles.tBytes[t]);
		if (bytesread < tiles.tBytes[t])
			derror();
        unsigned long dstLen = tileBytes;
        int err =
            uncompress(uBuffer.data() + tileRowBytes, &dstLen, cBuffer.data(), (unsigned long)tiles.tBytes[t]);
        if (err != Z_OK)
        {
          throw LIBRAW_EXCEPTION_DECODE_RAW;
          return;
        }
        else
        {
          int bytesps = ifd->bps >> 3;
          size_t rowsInTile = y + tiles.tileHeight > imgdata.sizes.raw_height ? imgdata.sizes.raw_height - y : tiles.tileHeight;
          size_t colsInTile = x + tiles.tileWidth > imgdata.sizes.raw_width ? imgdata.sizes.raw_width - x : tiles.tileWidth;

          for (size_t row = 0; row < rowsInTile; ++row) // do not process full tile if not needed
          {
              unsigned char *dst = uBuffer.data() + row * tiles.tileWidth * bytesps * ifd->samples;
              unsigned char *src = dst + tileRowBytes;
              DecodeFPDelta(src, dst, tiles.tileWidth / xFactor, ifd->samples * xFactor, bytesps);
              float lmax = expandFloats(dst, tiles.tileWidth * ifd->samples, bytesps);
            max = MAX(max, lmax);
            unsigned char *dst2 = (unsigned char *)&float_raw_image
                [((y + row) * imgdata.sizes.raw_width + x) * ifd->samples];
            memmove(dst2, dst, colsInTile * ifd->samples * sizeof(float));
          }
        }
      }
    }
  
  imgdata.color.fmaximum = max;

  // Set fields according to data format

  imgdata.rawdata.raw_alloc = float_raw_image;
  if (ifd->samples == 1)
  {
    imgdata.rawdata.float_image = float_raw_image;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 4;
  }
  else if (ifd->samples == 3)
  {
    imgdata.rawdata.float3_image = (float(*)[3])float_raw_image;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 12;
  }
  else if (ifd->samples == 4)
  {
    imgdata.rawdata.float4_image = (float(*)[4])float_raw_image;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 16;
  }

  if (imgdata.rawparams.options & LIBRAW_RAWOPTIONS_CONVERTFLOAT_TO_INT)
    convertFloatToInt(); // with default settings
}
#else
void LibRaw::deflate_dng_load_raw() { throw LIBRAW_EXCEPTION_DECODE_RAW; }
#endif

int LibRaw::is_floating_point()
{
  struct tiff_ifd_t *ifd = &tiff_ifd[0];
  while (ifd < &tiff_ifd[libraw_internal_data.identify_data.tiff_nifds] &&
         ifd->offset != libraw_internal_data.unpacker_data.data_offset)
    ++ifd;
  if (ifd == &tiff_ifd[libraw_internal_data.identify_data.tiff_nifds])
    return 0;

  return ifd->sample_format == 3;
}

int LibRaw::have_fpdata()
{
  return imgdata.rawdata.float_image || imgdata.rawdata.float3_image ||
         imgdata.rawdata.float4_image;
}

void LibRaw::convertFloatToInt(float dmin /* =4096.f */,
                               float dmax /* =32767.f */,
                               float dtarget /*= 16383.f */)
{
  int samples = 0;
  float *data = 0;
  void *orawalloc = imgdata.rawdata.raw_alloc;
  if (imgdata.rawdata.float_image)
  {
    samples = 1;
    data = imgdata.rawdata.float_image;
  }
  else if (imgdata.rawdata.float3_image)
  {
    samples = 3;
    data = (float *)imgdata.rawdata.float3_image;
  }
  else if (imgdata.rawdata.float4_image)
  {
    samples = 4;
    data = (float *)imgdata.rawdata.float4_image;
  }
  else
    return;

  INT64 pixcnt = INT64(imgdata.sizes.raw_height) * INT64(imgdata.sizes.raw_width)
	  * INT64(libraw_internal_data.unpacker_data.tiff_samples);

  if (pixcnt * sizeof(ushort) > INT64(imgdata.rawparams.max_raw_memory_mb) * 1024LL * 1024LL)
    throw LIBRAW_EXCEPTION_TOOBIG;

  ushort *raw_alloc =
#ifdef LIBRAW_CALLOC_RAWSTORE
		(ushort *)calloc(pixcnt, sizeof(ushort));
#else
		(ushort *)malloc(pixcnt * sizeof(ushort));
#endif
  float tmax = float(MAX(imgdata.color.maximum, 1));
  float datamax = imgdata.color.fmaximum;

  tmax = MAX(tmax, datamax);
  tmax = MAX(tmax, 1.f);

  float multip = 1.f;
  if (tmax < dmin || tmax > dmax)
  {
    imgdata.rawdata.color.fnorm = imgdata.color.fnorm = multip = dtarget / tmax;
    imgdata.rawdata.color.maximum = imgdata.color.maximum = unsigned(dtarget);
    imgdata.rawdata.color.black = imgdata.color.black =
        unsigned((float)imgdata.color.black * multip);
    for (int i = 0;
         i < int(sizeof(imgdata.color.cblack)/sizeof(imgdata.color.cblack[0]));
         i++)
      if (i != 4 && i != 5)
        imgdata.rawdata.color.cblack[i] = imgdata.color.cblack[i] =
            unsigned((float)imgdata.color.cblack[i] * multip);
  }
  else
    imgdata.rawdata.color.fnorm = imgdata.color.fnorm = 0.f;

  for (size_t i = 0; i < pixcnt; ++i)
  {
    float val = MAX(data[i], 0.f);
    raw_alloc[i] = (ushort)(val * multip);
  }

  if (samples == 1)
  {
    imgdata.rawdata.raw_alloc = imgdata.rawdata.raw_image = raw_alloc;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 2;
  }
  else if (samples == 3)
  {
    imgdata.rawdata.raw_alloc = imgdata.rawdata.color3_image =
        (ushort(*)[3])raw_alloc;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 6;
  }
  else if (samples == 4)
  {
    imgdata.rawdata.raw_alloc = imgdata.rawdata.color4_image =
        (ushort(*)[4])raw_alloc;
    imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
        imgdata.sizes.raw_width * 8;
  }
  if(orawalloc)
    free(orawalloc); // remove old allocation
  imgdata.rawdata.float_image = 0;
  imgdata.rawdata.float3_image = 0;
  imgdata.rawdata.float4_image = 0;
}

static
#if (defined(_MSC_VER) && !defined(__clang__))
_forceinline
#else
inline
#endif
void libraw_swap24(uchar *data, int len)
{
    for (int i = 0; i < len - 2; i += 3)
    {
        uchar t = data[i];
        data[i] = data[i + 2];
        data[i + 2] = t;
    }
}

static
#if (defined(_MSC_VER) && !defined(__clang__))
_forceinline
#else
inline
#endif
void libraw_swap32(uchar *data, int len)
{
    unsigned *d = (unsigned*)data;
    for (int i = 0; i < len / 4; i++)
    {
        unsigned x = d[i];
        d[i] = (x << 24) + ((x << 8) & 0x00FF0000) +
            ((x >> 8) & 0x0000FF00) + (x >> 24);
    }
}


void LibRaw::uncompressed_fp_dng_load_raw()
{
    int iifd = find_ifd_by_offset(libraw_internal_data.unpacker_data.data_offset);
    if (iifd < 0 || iifd > (int)libraw_internal_data.identify_data.tiff_nifds)
        throw LIBRAW_EXCEPTION_DECODE_RAW;
    struct tiff_ifd_t *ifd = &tiff_ifd[iifd];

    float *float_raw_image = 0;

    if (ifd->samples != 1 && ifd->samples != 3 && ifd->samples != 4)
        throw LIBRAW_EXCEPTION_DECODE_RAW; 

    if(imgdata.idata.filters && ifd->samples > 1)
      throw LIBRAW_EXCEPTION_DECODE_RAW;

    if ((int)libraw_internal_data.unpacker_data.tiff_samples != ifd->samples)
        throw LIBRAW_EXCEPTION_DECODE_RAW; // Wrong IFD

    int bytesps = (ifd->bps + 7) >> 3; // round to upper value

	if(bytesps < 1 || bytesps > 4)
      throw LIBRAW_EXCEPTION_DECODE_RAW;

    tile_stripe_data_t tiles;
    tiles.init(ifd, imgdata.sizes, libraw_internal_data.unpacker_data, libraw_internal_data.unpacker_data.order,
        libraw_internal_data.internal_data.input);

	// Max bytes: 2^16 raw width * 2^2 bytes/pixel * 2^2 channels = 2^20, so check against 2^22
	INT64 rowbytes = INT64(MAX(tiles.tileWidth, imgdata.sizes.raw_width)) * INT64(MAX(bytesps,4)) * INT64(ifd->samples);
	if(rowbytes > (1LL << 22))
      throw LIBRAW_EXCEPTION_TOOBIG;

	INT64 allocsz = INT64(tiles.tileCnt) * INT64(tiles.tileWidth) * INT64(tiles.tileHeight) * INT64(ifd->samples) * INT64(sizeof(float));
	if (allocsz > INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024))
		throw LIBRAW_EXCEPTION_TOOBIG;

    if (ifd->sample_format == 3)
        float_raw_image = (float *)calloc(allocsz,1);
    else
        throw LIBRAW_EXCEPTION_DECODE_RAW; // Only float supported

    bool difford = (libraw_internal_data.unpacker_data.order == 0x4949) == (ntohs(0x1234) == 0x1234);
    float max = 0.f;

    std::vector<uchar> rowbuf(tiles.tileWidth *sizeof(float) * ifd->samples,0); // line buffer for last tile in tile row

    for (size_t y = 0, t = 0; y < imgdata.sizes.raw_height; y += tiles.tileHeight)
    {
        for (unsigned x = 0; x < imgdata.sizes.raw_width  && t < (unsigned)tiles.tileCnt; x += tiles.tileWidth, ++t)
        {
			if (t >= tiles.tOffsets.size()) // should not happen but check anyway
				throw LIBRAW_EXCEPTION_IO_CORRUPT;
			libraw_internal_data.internal_data.input->seek(tiles.tOffsets[t], SEEK_SET);
            size_t rowsInTile = y + tiles.tileHeight > imgdata.sizes.raw_height ? imgdata.sizes.raw_height - y : tiles.tileHeight;
            size_t colsInTile = x + tiles.tileWidth > imgdata.sizes.raw_width ? imgdata.sizes.raw_width - x : tiles.tileWidth;

			// inrowbytes is less then 2^22 (see above) so conversion to int is safe
            size_t inrowbytes = colsInTile * bytesps * ifd->samples;
            int fullrowbytes = tiles.tileWidth *bytesps * ifd->samples;
            size_t outrowbytes = colsInTile * sizeof(float) * ifd->samples;

            for (size_t row = 0; row < rowsInTile; ++row) // do not process full tile if not needed
            {
                unsigned char *dst = fullrowbytes > int(inrowbytes) ? rowbuf.data(): // last tile in row, use buffer
                    (unsigned char *)&float_raw_image
                    [((y + row) * imgdata.sizes.raw_width + x) * ifd->samples];
                int bytesread = libraw_internal_data.internal_data.input->read(dst, 1, fullrowbytes);
				if (bytesread < fullrowbytes)
					derror();
                if (bytesps == 2 && difford)
                    libraw_swab(dst, fullrowbytes);
                else if (bytesps == 3 && (libraw_internal_data.unpacker_data.order == 0x4949)) // II-16bit
                    libraw_swap24(dst, fullrowbytes);
                if (bytesps == 4 && difford)
                    libraw_swap32(dst, fullrowbytes);

                float lmax = expandFloats(
                    dst,
                    tiles.tileWidth * ifd->samples,
                    bytesps);
                if (fullrowbytes > int(inrowbytes)) // last tile in row: copy buffer to destination
                    memmove(&float_raw_image[((y + row) * imgdata.sizes.raw_width + x) * ifd->samples], dst, outrowbytes);
                max = MAX(max, lmax);
            }
        }
    }

    imgdata.color.fmaximum = max;

    // setup outpuf fields
    imgdata.rawdata.raw_alloc = float_raw_image;
    if (ifd->samples == 1)
    {
        imgdata.rawdata.float_image = float_raw_image;
        imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
            imgdata.sizes.raw_width * 4;
    }
    else if (ifd->samples == 3)
    {
        imgdata.rawdata.float3_image = (float(*)[3])float_raw_image;
        imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
            imgdata.sizes.raw_width * 12;
    }
    else if (ifd->samples == 4)
    {
        imgdata.rawdata.float4_image = (float(*)[4])float_raw_image;
        imgdata.rawdata.sizes.raw_pitch = imgdata.sizes.raw_pitch =
            imgdata.sizes.raw_width * 16;
    }

    if (imgdata.rawparams.options & LIBRAW_RAWOPTIONS_CONVERTFLOAT_TO_INT)
        convertFloatToInt();  
}

#ifdef USE_JXL
/* ALPA patch(JPEG XL DNG,2026-11-10)—— DNG 1.7 Compression = 52546 的分块解码,不经 Adobe DNG SDK。
 * 原版只有 jxl_dng_load_raw_placeholder(dng.cpp,恒抛 UNSUPPORTED_FORMAT,真解码在 DNG SDK 里);本仓不带 DNG SDK,
 * 定义 USE_JXL 时由这里接管同名函数(dng.cpp 那份改成 #ifndef USE_JXL),不改 LibRaw 头文件 / 对象布局。
 * 解法对照 Adobe DNG SDK 1.7 dng_jxl.cpp 的解码端:不重设色彩(保持码流原始编码,raw 本就是线性)、
 * 不转方向(KeepOrientation)、色彩平面只认 1(CFA)/ 3(LinearRaw)、整数按 UINT16 读、浮点暂不接。
 * 与它的一处差异:这里按 DNG 头 BitsPerSample 的整数口径取值(与码流同位深时即码流原始整数),不让 libjxl 按 16 位满幅放大 ——
 * DNG 的 BlackLevel / WhiteLevel 是按这个口径写的。像素经 adobe_copy_pixel 落进 raw_image(CFA)/ image(LinearRaw),
 * 与 packed / lossless DNG 同一条路(线性化表 curve[] 照用)。 */
#include <jxl/decode.h>

void LibRaw::jxl_dng_load_raw_placeholder()
{
  int iifd = find_ifd_by_offset(libraw_internal_data.unpacker_data.data_offset);
  if (iifd < 0 || iifd > (int)libraw_internal_data.identify_data.tiff_nifds)
    throw LIBRAW_EXCEPTION_DECODE_RAW;
  struct tiff_ifd_t *ifd = &tiff_ifd[iifd];
  const unsigned samples = (unsigned)ifd->samples;
  if (samples != 1 && samples != 3)
    throw LIBRAW_EXCEPTION_DECODE_RAW;
  if (libraw_internal_data.unpacker_data.tiff_samples != samples)
    throw LIBRAW_EXCEPTION_DECODE_RAW;
  if (ifd->sample_format == 3)
    throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT; // 浮点 JXL DNG 暂不接(手机 / 相机未见)
  // DNG 头声明的位深:BlackLevel / WhiteLevel / 线性化表都按它的整数口径写。码流位深与它不同时(写入端把 12 位数据
  // 标成 16 位 DNG,或反过来),按 DNG 的位深取值(JXL_BIT_DEPTH_CUSTOM,libjxl 按比例换算),而不是按码流原样 ——
  // 否则整张图按 2^(差) 倍错亮 / 错暗且不报错(复读 P2)。相等时与按码流原样取逐位相同。
  const unsigned dngBits = (unsigned)ifd->bps;
  if (dngBits < 1 || dngBits > 16)
    throw LIBRAW_EXCEPTION_DECODE_RAW;

  tile_stripe_data_t tiles;
  tiles.init(ifd, imgdata.sizes, libraw_internal_data.unpacker_data, libraw_internal_data.unpacker_data.order,
             libraw_internal_data.internal_data.input);
  if (tiles.tBytes.size() < 1)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
  const INT64 memLimit = INT64(imgdata.rawparams.max_raw_memory_mb) * 1024LL * 1024LL;
  if (tiles.maxBytesInTile <= 0 || tiles.maxBytesInTile > memLimit || tiles.maxBytesInTile >= (1LL << 31))
    throw LIBRAW_EXCEPTION_TOOBIG;
  const INT64 tilePixelBytes = INT64(tiles.tileWidth) * INT64(tiles.tileHeight) * INT64(samples) * 2;
  if (tilePixelBytes <= 0 || tilePixelBytes > memLimit)
    throw LIBRAW_EXCEPTION_TOOBIG;

  std::vector<uchar> cBuffer((size_t)tiles.maxBytesInTile);
  std::vector<ushort> pix;
  JxlDecoder *dec = JxlDecoderCreate(NULL);
  if (!dec)
    throw LIBRAW_EXCEPTION_ALLOC;
  try
  {
    for (unsigned y = 0, t = 0; y < tiles.tilesV; y++)
    {
      for (unsigned x = 0; x < tiles.tilesH; x++, t++)
      {
        checkCancel();
        if (tiles.tBytes[t] <= 0 || tiles.tBytes[t] > tiles.maxBytesInTile)
          throw LIBRAW_EXCEPTION_IO_CORRUPT;
        libraw_internal_data.internal_data.input->seek(tiles.tOffsets[t], SEEK_SET);
        if (libraw_internal_data.internal_data.input->read(cBuffer.data(), 1, (size_t)tiles.tBytes[t]) != tiles.tBytes[t])
          throw LIBRAW_EXCEPTION_IO_EOF;

        JxlDecoderReset(dec);
        if (JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS ||
            JxlDecoderSetKeepOrientation(dec, JXL_TRUE) != JXL_DEC_SUCCESS ||
            JxlDecoderSetInput(dec, cBuffer.data(), (size_t)tiles.tBytes[t]) != JXL_DEC_SUCCESS)
          throw LIBRAW_EXCEPTION_DECODE_RAW;
        JxlDecoderCloseInput(dec);

        JxlBasicInfo info;
        const JxlPixelFormat fmt = {samples, JXL_TYPE_UINT16, JXL_NATIVE_ENDIAN, 0};
        bool haveInfo = false, done = false;
        while (!done)
        {
          switch (JxlDecoderProcessInput(dec))
          {
          case JXL_DEC_BASIC_INFO:
            if (JxlDecoderGetBasicInfo(dec, &info) != JXL_DEC_SUCCESS)
              throw LIBRAW_EXCEPTION_DECODE_RAW;
            // 平面数必须等于 DNG 的 SamplesPerPixel;不认浮点 / > 16 位整数;一块不能大于 TileWidth × TileLength
            if (info.num_color_channels != samples || info.exponent_bits_per_sample != 0 ||
                info.bits_per_sample < 1 || info.bits_per_sample > 16 ||
                info.xsize < 1 || info.ysize < 1 || info.xsize > tiles.tileWidth || info.ysize > tiles.tileHeight)
              throw LIBRAW_EXCEPTION_DECODE_RAW;
            haveInfo = true;
            break;
          case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
          {
            if (!haveInfo)
              throw LIBRAW_EXCEPTION_DECODE_RAW;
            // 位深须在设好输出缓冲之后再设(libjxl decode.h:JxlDecoderSetImageOutBitDepth 的调用约束)
            const JxlBitDepth depth = info.bits_per_sample == dngBits
                                          ? JxlBitDepth{JXL_BIT_DEPTH_FROM_CODESTREAM, 0, 0}
                                          : JxlBitDepth{JXL_BIT_DEPTH_CUSTOM, dngBits, 0};
            size_t need = 0;
            if (JxlDecoderImageOutBufferSize(dec, &fmt, &need) != JXL_DEC_SUCCESS ||
                need != size_t(info.xsize) * info.ysize * samples * 2)
              throw LIBRAW_EXCEPTION_DECODE_RAW;
            pix.resize(need / 2);
            if (JxlDecoderSetImageOutBuffer(dec, &fmt, pix.data(), need) != JXL_DEC_SUCCESS ||
                JxlDecoderSetImageOutBitDepth(dec, &depth) != JXL_DEC_SUCCESS)
              throw LIBRAW_EXCEPTION_DECODE_RAW;
            break;
          }
          case JXL_DEC_FULL_IMAGE:
            done = true; // 一块只取第一帧
            break;
          case JXL_DEC_SUCCESS:
            if (!haveInfo || pix.empty())
              throw LIBRAW_EXCEPTION_DECODE_RAW;
            done = true;
            break;
          default: // JXL_DEC_ERROR / NEED_MORE_INPUT(输入已关)/ 其它
            throw LIBRAW_EXCEPTION_DECODE_RAW;
          }
        }
        const unsigned row0 = y * tiles.tileHeight, col0 = x * tiles.tileWidth;
        for (unsigned r = 0; r < info.ysize && row0 + r < imgdata.sizes.raw_height; r++)
        {
          ushort *rp = pix.data() + size_t(r) * info.xsize * samples;
          for (unsigned c = 0; c < info.xsize && col0 + c < imgdata.sizes.raw_width; c++)
            adobe_copy_pixel(row0 + r, col0 + c, &rp);
        }
      }
    }
  }
  catch (...)
  {
    JxlDecoderDestroy(dec);
    throw;
  }
  JxlDecoderDestroy(dec);
}
#endif
