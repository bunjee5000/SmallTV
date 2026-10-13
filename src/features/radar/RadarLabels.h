#pragma once
#include <stdint.h>

struct RadarLabelBox { int16_t x, y, w, h; };
inline bool radarLabelHit(const RadarLabelBox& a, const RadarLabelBox& b) {
  return a.x+a.w > b.x && b.x+b.w > a.x && a.y+a.h > b.y && b.y+b.h > a.y;
}
inline bool radarPlaceLabel(int x, int y, int gap, int width, int height,
                            const RadarLabelBox* placed, uint8_t count,
                            int ignore, RadarLabelBox& out, int top = 2) {
  if (width <= 0 || height <= 0 || width > 236 || height > 238-top) return false;
  // Try sides, above/below, then diagonals. The caller can reserve its header.
  // Check the whole multiline box after clamping, not just the first line.
  for (int attempt=0; attempt<8; ++attempt) {
    int lx = attempt==0 ? x+gap : attempt==1 ? x-gap-width :
             attempt<4 ? x-width/2 : attempt%2==0 ? x+gap : x-gap-width;
    int ly = attempt<2 ? y-height/2 : attempt==2 || attempt>=6 ? y-gap-height : y+gap;
    if (lx<2) lx=2;
    if (lx+width>238) lx=238-width;
    if (ly<top) ly=top;
    if (ly+height>238) ly=238-height;
    RadarLabelBox box={(int16_t)lx,(int16_t)ly,(int16_t)width,(int16_t)height};
    RadarLabelBox marker={(int16_t)(x-gap),(int16_t)(y-gap),(int16_t)(2*gap),(int16_t)(2*gap)};
    if (radarLabelHit(box,marker)) continue;
    bool clash=false;
    for(uint8_t j=0;j<count;++j) if(j!=ignore && radarLabelHit(box,placed[j])) { clash=true; break; }
    if (!clash) { out=box; return true; }
  }
  return false;
}
