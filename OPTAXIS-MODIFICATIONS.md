# Optaxis 对 LibRaw 0.22.2 的改动

本仓库是光轴 Optaxis 安卓版所用 LibRaw 的源码,按 **CDDL-1.0** 使用与再分发(LibRaw 为 LGPL-2.1 / CDDL-1.0 双许可)。
第一个 commit 是上游 LibRaw 0.22.2 原版;第二个 commit 是我们的改动,改过的文件:

- `libraw/libraw_const.h`
- `libraw/libraw_types.h`
- `src/metadata/identify.cpp`
- `src/metadata/tiff.cpp`

改动目的:DNG 只带 AsShotWhiteXY、不带 AsShotNeutral 时,保留该白点坐标与所选 XYZ→相机矩阵,供应用还原拍摄时白平衡。
所有改动处均以 `// ALPA patch` 注释标出。

This repository contains the LibRaw source used by the Optaxis Android app, distributed under CDDL-1.0.
The first commit is unmodified upstream LibRaw 0.22.2; the second commit contains our modifications (marked `// ALPA patch`).
