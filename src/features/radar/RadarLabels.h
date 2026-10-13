#pragma once
#include <stdint.h>

struct RadarLabelBox { int16_t x, y, w, h; };
inline bool radarLabelHit(const RadarLabelBox& a, const RadarLabelBox& b) {
  return a.x+a.w > b.x && b.x+b.w > a.x && a.y+a.h > b.y && b.y+b.h > a.y;
}
inline bool radarPlaceLabel(int x, int y, int gap, int width, int height,
                            const RadarLabelBox* placed, uint8_t count,
                            int ignore, RadarLabelBox& out) {
  if (width > 236 || height > 236) return false;
  // Try both sides, then above and below the marker. Clamp both axes.
  for (int attempt=0; attempt<4; ++attempt) {
    int lx = attempt==0 ? x+gap : attempt==1 ? x-gap-width : x-width/2;
    int ly = attempt<2 ? y-8 : attempt==2 ? y-gap-height : y+gap;
    if (lx<2) lx=2;
    if (lx+width>238) lx=238-width;
    if (ly<2) ly=2;
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
