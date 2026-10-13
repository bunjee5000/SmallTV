#include "Arduino.h"
#include "features/radar/RadarFeed.h"
#include <assert.h>
#include <map>
#include <sstream>
#include <iostream>

class TrackingAllocator : public ArduinoJson::Allocator {
 public:
  std::map<void*, size_t> allocations;
  size_t used = 0, peak = 0;
  void* allocate(size_t n) override { void* p = malloc(n); if (p) { allocations[p]=n; used+=n; if (used>peak) peak=used; } return p; }
  void deallocate(void* p) override { if (p) { used-=allocations.at(p); allocations.erase(p); free(p); } }
  void* reallocate(void* p, size_t n) override {
    if (!p) return allocate(n);
    size_t old=allocations.at(p); void* q=realloc(p,n);
    if (q) { allocations.erase(p); allocations[q]=n; used=used-old+n; if(used>peak) peak=used; }
    return q;
  }
};
struct Reader {
  const std::string& text; size_t offset=0;
  explicit Reader(const std::string& s):text(s){}
  int read() { return offset<text.size() ? (unsigned char)text[offset++] : -1; }
};
static bool parse(const std::string& text, Aircraft* out, RadarFeedResult& result, TrackingAllocator& alloc, int minAlt=0) {
  Reader reader(text); RadarFeedSettings home={52,10,(uint16_t)minAlt};
  return radarParseFeed(reader,home,out,MAX_AIRCRAFT,result,[](){},&alloc);
}
static std::string feed(unsigned count) {
  std::ostringstream s; s<<"{\"now\":1791815000.25,\"message\":\"mentions \\\"ac\\\" [ ]\",\"ac\":[";
  for(unsigned i=0;i<count;i++) {
    if(i)s<<',';
    s<<"{\"lat\":"<<52+(count-i)*.0001<<",\"lon\":10,\"alt_baro\":35000,\"track\":90,\"gs\":420,\"flight\":\"TK"<<i<<"   \",\"ignored\":{\"nested\":[1,2,3]}}";
  }
  s<<"],\"total\":"<<count<<",\"other\":[true,null,{\"ac\":[{}]}]}";return s.str();
}
int main() {
  Aircraft out[MAX_AIRCRAFT]; RadarFeedResult result;
  size_t boundedPeak=0;
  for(unsigned n:{24u,100u,1000u}) {
    std::string text=feed(n); TrackingAllocator alloc;
    assert(parse(text,out,result,alloc)); assert(result.count==24 && result.seen==n && result.found);
    assert(alloc.used==0); if(!boundedPeak)boundedPeak=alloc.peak; else assert(alloc.peak<=boundedPeak+128);
    for(int i=1;i<24;i++) assert(out[i-1].distKm<=out[i].distKm);
    assert(out[0].altFt==35000 && out[0].track==90 && out[0].gs==420);
    TrackingAllocator old;
    { JsonDocument filter(&old),doc(&old); auto f=filter["ac"][0].to<JsonObject>();
      for(const char* name:{"lat","lon","track","gs","flight","hex","alt_baro"})f[name]=true;
      assert(!deserializeJson(doc,text,DeserializationOption::Filter(filter))); }
    std::cout<<n<<" aircraft: old JSON peak "<<old.peak<<" B; streaming JSON "<<alloc.peak
             <<" B + "<<sizeof(out)<<" B scratch (native allocator measurements)\n";
  }
  for(std::string bad:{"{\"ac\":[{}", "{\"ac\":[{},]}", "{\"ac\":[]", "{\"ac\":[],\"x\":}",
                       "{\"ac\":[],\"ac\":[]}", "{\"ac\":null}", "{\"ac\":[{}],\"x\":[1,2"}) {
    TrackingAllocator alloc; assert(!parse(bad,out,result,alloc)); assert(!alloc.used);
  }
  { TrackingAllocator alloc; assert(parse("{\"foo\":{\"ac\":[{}]}}",out,result,alloc)); assert(!result.found); }
  { TrackingAllocator alloc; assert(parse("{\"a\\u0063\":[]}",out,result,alloc)); assert(result.found && !result.count); }
  { TrackingAllocator alloc; assert(parse(feed(100),out,result,alloc,40000)); assert(!result.count && result.seen==100); }
  { TrackingAllocator alloc;
    assert(parse("{\"ac\":[{\"lat\":52,\"lon\":10,\"flight\":\"   \",\"hex\":\"abc123\"},null,17]}",out,result,alloc));
    assert(result.seen==3 && result.count==1 && !strcmp(out[0].callsign,"abc123")); }
  std::cout<<"Radar feed ordering, filtering, metadata, empty/malformed/truncated input: PASS\n";
}
