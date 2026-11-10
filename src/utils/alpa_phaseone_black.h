// alpa_phaseone_black.h —— ALPA patch:飞思暗场降噪 IIQ 的实测黑电平(判据单源)
//
// 放在 LibRaw 树里是有意的:bin/publish-libraw-source.sh 只发布这个目录,判据得随改动源码一起公开(CDDL §3.1)。
// 谁在用(改判据只改这里,两个产品自动一致):
//   · app 双端:同目录 phaseone_processing.cpp 的 ALPA patch(安卓 libraw_r.so 与 iOS ALPALibRaw 编同一份)
//   · macOS 转换器:ALPAHEICConverter/Vendor/ALPALibRaw/ALPALibRaw.cpp(它的 LibRaw 副本按 VENDOR.md 一行不改,
//     所以在包装层调本函数,命中时把原始数据抬高「标签 − 实测」再交给 LibRaw,与 app 同一个数;
//     唯一例外见那边注释:校正表非空时的饱和像素可能差几个码值)
//   · tools/raw-calib/raw_dump(编的是 app 那份 LibRaw)
//
// 病:飞思开长曝光暗场降噪(Black Reference / dark frame)时,机身已把暗场减掉,写进文件的数据底座只剩约 150,
// 但 IIQ 的 t_black 标签仍写 1024。照标签再减 1024 ⟹ 信号弱的通道(暮色里的红)整片被截成 0,打开即严重偏青。
// 样张 P0001372(IQ4 150MP,3600 s,传感器 41.9 °C):左遮光区中位数 147,有效区 0.4–0.7 % 像素为 0;
// 飞思自家软件与文件内嵌预览都正常 ⟹ 是我们多减了一次,不是相机白平衡。
//
// 判据(两个独立信号同时成立才返回实测值,否则原样返回标签值):
//   ① 左侧遮光区(不感光列)中位数比 t_black 低超过 ALPA_P1_BLACK_MARGIN —— 正常文件实测 1016–1029 / 标签 1024;
//   ② 有效区自身的暗部 p0.1 也低于 t_black − 余量 —— 数据确实落在标签之下,不是遮光区布局特殊。
//      IQ250(CF011554)的遮光区约 32768,①不成立 ⟹ 不受影响。
// 白点不用跟着改:暗场后的饱和值 ≈ 0xffff − (1024 − 实测),减去实测后 ≈ 0xfffc − 1024 = 原 maximum(P0001372 实测最大 64751)。
//
// 输入是解包后、减黑电平之前的原始像素(行跨度 = raw_width 个 uint16),边距 / 有效区尺寸取 LibRaw 的 sizes。
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

static const int ALPA_P1_BLACK_MARGIN = 64;

// gpu-first: cpu —— 在 LibRaw 解码内部、unpack 之后与 LibRaw 自己的 CPU 减黑电平之前运行,产物只是喂回那次 CPU 减法的一个整数;
// 此时数据只在 LibRaw 的 CPU 缓冲里、后续去马赛克也在 CPU,没有 GPU 管线可接,为一个中位数上传约 300 MB 原始数据只会更慢。
// 抽样统计非逐像素处理:实测(M 系 Mac,P0001372)2.0 ms / 次,正常文件 0.3 ms(第一步即返回),整次 dcraw_process 4213 ms。
static inline int alpa_p1_measured_black(const uint16_t *src, int raw_width, int raw_height, int top, int left,
                                         int width, int height, int tagBlack)
{
  if (!src || tagBlack <= ALPA_P1_BLACK_MARGIN || left < 16 || width <= 0 || height <= 0 ||
      top + height > raw_height || left + width > raw_width)
    return tagBlack;
  std::vector<unsigned> hist(65536, 0);
  size_t n = 0;
  // gpu-first: cpu —— 见函数头:LibRaw 解码内 CPU 阶段的抽样统计,产物是喂回 CPU 减法的一个整数(实测 2 ms)
  for (int row = top; row < top + height; row += 2)
    for (int col = 4; col < left - 4; col++)
    {
      hist[src[size_t(row) * raw_width + col]]++;
      n++;
    }
  if (n < 10000)
    return tagBlack;
  size_t acc = 0;
  int masked = 0;
  for (int v = 0; v < 65536; v++)
  {
    acc += hist[v];
    if (acc * 2 >= n) { masked = v; break; }
  }
  if (masked + ALPA_P1_BLACK_MARGIN >= tagBlack)
    return tagBlack; // ①不成立:正常文件
  std::fill(hist.begin(), hist.end(), 0u);
  size_t m = 0;
  // gpu-first: cpu —— 见函数头:LibRaw 解码内 CPU 阶段的抽样统计,产物是喂回 CPU 减法的一个整数(实测 2 ms)
  for (int row = top; row < top + height; row += 7)
    for (int col = left; col < left + width; col += 7)
    {
      hist[src[size_t(row) * raw_width + col]]++;
      m++;
    }
  acc = 0;
  int low = 65535;
  for (int v = 0; v < 65536; v++)
  {
    acc += hist[v];
    if (acc * 1000 >= m) { low = v; break; }
  }
  if (low + ALPA_P1_BLACK_MARGIN >= tagBlack)
    return tagBlack; // ②不成立:有效区并没有落到标签之下
  return masked;
}
