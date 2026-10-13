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
  bool clipGeometry = false;
  int textMargin = 0;
  bool checkTextOverlap = false;
  struct TextBox { int x, y, w, h; };
  std::vector<TextBox> textBoxes;
  std::vector<std::string> strings;
  void setTextSize(int s) { size=s; }
  void setTextColor(uint16_t c) { color=c; }
  void setCursor(int a,int b) { x=a; y=b; }
  void setTextWrap(bool) {}
  void drawPixel(int a,int b,uint16_t c) {
    if(clipGeometry && (a<0 || a>=240 || b<0 || b>=240)) return;
    assert(a>=0 && a<240 && b>=0 && b<240);
    pixels[b*240+a]=c;
  }
  void fillRect(int a,int b,int w,int h,uint16_t c) {
    for(int j=0;j<h;j++) for(int i=0;i<w;i++) drawPixel(a+i,b+j,c);
  }
  void fillScreen(uint16_t c) { std::fill(pixels.begin(),pixels.end(),c); textRows=0; strings.clear(); textBoxes.clear(); }
  // Matches Arduino_GFX::charBounds for the installed fixed 6x8 font.
  void getTextBounds(const char* value,int16_t a,int16_t b,int16_t* x1,int16_t* y1,uint16_t* w,uint16_t* h) {
    *x1=a; *y1=b; *w=strlen(value)*6*size; *h=*value ? 8*size : 0;
  }
  void fillTriangle(int ax,int ay,int bx,int by,int cx,int cy,uint16_t c) {
    auto cross=[](int x1,int y1,int x2,int y2,int px,int py) { return (px-x1)*(y2-y1)-(py-y1)*(x2-x1); };
    int minX=std::min(ax,std::min(bx,cx)),maxX=std::max(ax,std::max(bx,cx));
    int minY=std::min(ay,std::min(by,cy)),maxY=std::max(ay,std::max(by,cy));
    for(int j=minY;j<=maxY;++j) for(int i=minX;i<=maxX;++i) {
      int a=cross(ax,ay,bx,by,i,j),b=cross(bx,by,cx,cy,i,j),d=cross(cx,cy,ax,ay,i,j);
      if((a>=0 && b>=0 && d>=0)||(a<=0 && b<=0 && d<=0)) drawPixel(i,j,c);
    }
  }
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
    int16_t bx,by; uint16_t bw,bh;
    getTextBounds(value,x,y,&bx,&by,&bw,&bh);
    if(!(bx>=textMargin && bx+bw<=240-textMargin && by>=textMargin && by+bh<=240-textMargin))
      fprintf(stderr,"Text outside margin: '%s' at %d,%d, size %u x %u\n",value,bx,by,bw,bh);
    assert(bx>=textMargin && bx+bw<=240-textMargin && by>=textMargin && by+bh<=240-textMargin);
    if(checkTextOverlap && bw && bh) for(const auto& box:textBoxes)
      assert(!(bx<box.x+box.w && bx+bw>box.x && by<box.y+box.h && by+bh>box.y));
    textBoxes.push_back({bx,by,bw,bh});
    ++textRows;
    strings.emplace_back(value);
    for(;*value;value++) { unsigned ch=(unsigned char)*value;
      assert((ch>=32 && ch<127) || ch==129 || ch==132 || ch==148 || ch==248);
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
