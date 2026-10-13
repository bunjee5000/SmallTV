// Exercise the actual static icon and flash labels at controlled elongations.
// Including the production TU keeps test hooks out of the firmware.
#include "../src/Dashboard.cpp"
#include <assert.h>
#include <string>

static Arduino_GFX display;
static DashboardWeather weather;
time_t dashboardTestEpoch=1791815820;
Arduino_GFX* gfxDev() { return &display; }
uint16_t gfxTint(uint16_t c) { return c; }
const DashboardWeather& dashboardWeather() { return weather; }
bool dashboardWeatherStale() { return false; }
void dashboardWeatherApply(const DashboardSettings&) {}

struct Light { int left=0, right=0, total=0, lit=0; };
static Light inspect() {
  Light light;
  for(int y=-14;y<=14;++y) for(int x=-14;x<=14;++x) {
    if(x*x+y*y>14*14) continue;
    uint16_t pixel=display.pixels[(110+y)*240+180+x];
    if(pixel==0x4208) continue; // exclude the dim one-pixel outline
    assert(pixel==0x1082 || pixel==0xffff);
    ++light.total;
    if(pixel==0xffff) { ++light.lit; light.left+=x<0; light.right+=x>0; }
  }
  return light;
}
int main(int argc,char** argv) {
  display.textMargin=4; display.checkTextOverlap=true;
  const char* expected[][2]={{"Neumond",""},{"Zun.","Sichel"},{"Erstes","Viertel"},{"Zun.","Mond"},
    {"Vollmond",""},{"Abn.","Mond"},{"Letztes","Viertel"},{"Abn.","Sichel"}};
  for(int phase=0;phase<8;++phase) {
    display.fillScreen(0); moonIcon(&display,phase/8.0f);
    Light light=inspect();
    if(phase==0) assert(light.lit==0);
    if(phase==4) assert(light.lit==light.total);
    if(phase>0 && phase<4) assert(light.right>light.left);
    if(phase>4) assert(light.left>light.right);
    for(int line=0;line<2;++line) {
      assert(!strcmp(moonNames[phase][line],expected[phase][line]));
      if(*expected[phase][line]) centered(&display,180,130+20*line,2,0xffff,moonNames[phase][line],true);
    }
    if(argc>1) display.save((std::string(argv[1])+".phase-"+std::to_string(phase)+".ppm").c_str());
  }
  for(int sample=0;sample<=400;++sample) {
    float cycle=sample/400.0f;
    display.fillScreen(0); moonIcon(&display,cycle);
    Light light=inspect();
    float expectedFraction=(1-cosf(cycle*2*PI))/2;
    assert(fabsf((float)light.lit/light.total-expectedFraction)<0.04f);
    if(cycle>0 && cycle<0.5f && light.lit) assert(light.right>=light.left);
    if(cycle>0.5f && cycle<1 && light.lit) assert(light.left>=light.right);
    auto original=display.pixels;
    display.fillScreen(0); moonIcon(&display,1-cycle);
    for(int y=-14;y<=14;++y) for(int x=-14;x<=14;++x)
      assert(original[(110+y)*240+180+x]==display.pixels[(110+y)*240+180-x]);
  }
  puts("Moon renderer: all 8 German labels, 401 elongations, mirrored sides, new/full endpoints and illumination within 4 percentage points PASS.");
}
