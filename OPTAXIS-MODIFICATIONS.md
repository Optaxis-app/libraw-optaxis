# Optaxis 对 LibRaw master@a9a14b1 的改动

本仓库是光轴 Optaxis 安卓版所用 LibRaw 的源码,按 **CDDL-1.0** 使用与再分发(LibRaw 为 LGPL-2.1 / CDDL-1.0 双许可)。
第一个 commit 是上游 LibRaw master@a9a14b1 原版;第二个 commit 是我们的改动,改过的文件:

- `internal/libraw_internal_funcs.h`
- `libraw/libraw_const.h`
- `libraw/libraw_datastream.h`
- `libraw/libraw_types.h`
- `src/utils/alpa_phaseone_black.h`
- `src/utils/decoder_info.cpp`
- `src/utils/phaseone_processing.cpp`
- `src/postprocessing/mem_image.cpp`
- `src/decoders/load_mfbacks.cpp`
- `src/decoders/fp_dng.cpp`
- `src/decoders/dng.cpp`
- `src/metadata/identify.cpp`
- `src/metadata/tiff.cpp`

改动目的:
- DNG 只带 AsShotWhiteXY、不带 AsShotNeutral 时,保留该白点坐标与所选 XYZ→相机矩阵,供应用还原拍摄时白平衡。
- 飞思 IIQ 解压与减黑电平按行并行(结果逐位不变)。
- JPEG XL 压缩的 DNG(DNG 1.7,Compression 52546):定义 USE_JXL 时用 libjxl 解码分块(`src/decoders/fp_dng.cpp` 末尾),
  原占位实现包进 `#ifndef USE_JXL`(`src/decoders/dng.cpp`),解码器标志同 packed DNG(`src/utils/decoder_info.cpp`)。
- 飞思开长曝光暗场降噪的 IIQ,数据已减过暗场而 t_black 标签仍为 1024:按遮光区实测黑电平减,不再重复扣除
  (判据见 `src/utils/alpa_phaseone_black.h`)。
所有改动处均以 `// ALPA patch` 注释标出。

This repository contains the LibRaw source used by the Optaxis Android app, distributed under CDDL-1.0.
The first commit is unmodified upstream LibRaw master@a9a14b1; the second commit contains our modifications (marked `// ALPA patch`).
