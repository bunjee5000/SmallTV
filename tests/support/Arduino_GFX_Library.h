#pragma once
#include "Arduino.h"
#include <font/glcdfont.h> // use the firmware's installed font, not a replica
#include <fstream>
#include <vector>
#include <assert.h>

class Arduino_GFX {
 public:
  std::vector<uint16_t> pixels = std::vector<uint16_t>(240*240);
  unsigned textRows = 0;
  int x = 0, y = 0, size = 1;
  uint16_t color = 0xffff;
  void setTextSize(int s) { size=s; }
  void setTextColor(uint16_t c) { color=c; }
  void setCursor(int a,int b) { x=a; y=b; }
  void setTextWrap(bool) {}
  void drawPixel(int a,int b,uint16_t c) {
    assert(a>=0 && a<240 && b>=0 && b<240);
    pixels[b*240+a]=c;
  }
  void fillRect(int a,int b,int w,int h,uint16_t c) {
    for(int j=0;j<h;j++) for(int i=0;i<w;i++) drawPixel(a+i,b+j,c);
  }
  void fillScreen(uint16_t c) { std::fill(pixels.begin(),pixels.end(),c); textRows=0; }
  void drawFastHLine(int a,int b,int w,uint16_t c) { fillRect(a,b,w,1,c); }
  void drawFastVLine(int a,int b,int h,uint16_t c) { fillRect(a,b,1,h,c); }
  void drawLine(int a,int b,int endX,int endY,uint16_t c) {
    int dx=abs(endX-a),sx=a<endX?1:-1,dy=-abs(endY-b),sy=b<endY?1:-1,err=dx+dy;
    for(;;) { drawPixel(a,b,c); if(a==endX && b==endY) break;
      int e=2*err; if(e>=dy){err+=dy;a+=sx;} if(e<=dx){err+=dx;b+=sy;} }
  }
  void fillCircle(int a,int b,int r,uint16_t c) {
    for(int j=-r;j<=r;j++) { int half=(int)sqrt((double)(r*r-j*j)); drawFastHLine(a-half,b+j,2*half+1,c); }
  }
  void drawCircle(int a,int b,int r,uint16_t c) {
    int px=r,py=0,err=1-r;
    while(px>=py) {
      drawPixel(a+px,b+py,c);drawPixel(a+py,b+px,c);drawPixel(a-py,b+px,c);drawPixel(a-px,b+py,c);
      drawPixel(a-px,b-py,c);drawPixel(a-py,b-px,c);drawPixel(a+py,b-px,c);drawPixel(a+px,b-py,c);
      py++; if(err<0) err+=2*py+1; else{px--;err+=2*(py-px)+1;}
    }
  }
  void print(const char* value) {
    assert(x>=0 && x+(int)strlen(value)*6*size<=240 && y>=0 && y+8*size<=240);
    ++textRows;
    for(;*value;value++) { unsigned ch=(unsigned char)*value; assert(ch>=32 && ch<127);
      for(int i=0;i<5;i++) for(int j=0;j<8;j++) if(font[ch*5+i]&(1<<j)) fillRect(x+i*size,y+j*size,size,size,color);
      x+=6*size;
    }
  }
  void save(const char* path) {
    std::ofstream out(path,std::ios::binary);
    out<<"P6\n240 240\n255\n";
    for(uint16_t c:pixels) { out.put((char)(((c>>11)&31)*255/31));out.put((char)(((c>>5)&63)*255/63));out.put((char)((c&31)*255/31)); }
    assert(out.good());
  }
};
