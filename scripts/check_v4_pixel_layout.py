#!/usr/bin/env python3
"""Host checks of actual OLED layout helpers and physical button recognizer.
Run with python3 scripts/check_v4_pixel_layout.py (requires g++).
Not a full firmware build or a hardware rendering test.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
ui = (root / 'examples/companion_radio/ui-new/UITask.cpp').read_text()
a = ui.index('    // The OLED uses one content boundary')
b = ui.index('    if (_page == HomePage::FIRST) {', a)
layout = ui[a:b]
a = ui.index('  void returnToMain() {')
b = ui.index('\n#endif', a)
reset = ui[a:b]
mock = r'''
#pragma once
#include <stdint.h>
#include <cstring>
#define PROGMEM
#define pgm_read_byte(p) (*(p))
#define pgm_read_word(p) (*(p))
#define LOW 0
#define HIGH 1
#define INPUT 0
#define INPUT_PULLUP 1
#define INPUT_PULLDOWN 2
extern unsigned long clock_ms;
extern int button_level;
inline unsigned long millis() { return clock_ms; }
inline int digitalRead(int) { return button_level; }
inline int analogRead(int) { return button_level; }
inline void pinMode(int,int) {}
'''
source = r'''
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
#include "helpers/ui/DisplayDriver.h"
#include "helpers/ui/CyrillicFont.h"
#include "helpers/ui/MomentaryButton.h"
unsigned long clock_ms = 100;
int button_level = LOW;
uint16_t UIColor::window_bkg = 0;
struct Draw { int x,y,w; };
struct Display : DisplayDriver {
 std::vector<Draw> draws; int x=0,y=0;
 Display():DisplayDriver(128,64){}
 bool isOn() override{return true;}
 void turnOn() override{} void turnOff() override{} void clear() override{}
 void endFrame() override{} void startFrame(ColorVal) override{} void setTextSize(int) override{}
 void setColor(ColorVal) override{} void setCursor(int a,int b) override{x=a;y=b;}
 void print(const char* s) override {draws.push_back({x,y,getTextWidth(s)});}
 void fillRect(int,int,int,int) override{} void drawRect(int,int,int,int) override{}
 void drawXbm(int,int,const uint8_t*,int,int) override{}
 uint16_t getTextWidth(const char* s) override {
  int w=0; while(*s) {uint32_t cp; s=utf8Next(s,cp); int i=cyrillicGlyphIndex(cp);
   w+=i>=0?cyrAdvance(CYR_OLED,i):6;} return w;
 }
 void translateUTF8ToBlocks(char* d,const char* s,size_t n) override {
  translateUtf8KeepCyrillic(d,s,n);
 }
};
struct Home {
 enum HomePage {FIRST}; int _page=7,_sub_depth=3,stopped=0;
 bool _act_open=true,_settings_action_page=true,_radio_details=true,_radio_confirm=true,
 _scan_confirm=true,_conn_confirm=true,_power_confirm=true,_setup_edit=true,
 _adc_edit=true,_battery_confirm=true,_shutdown_init=true,_shutdown_apply=true;
 void stopFreqScan(){stopped++;}
 RESET_METHOD
};
void checkLayout() {
 Display display; bool compact_header=true;
 const int OLED_INSET=3;
 LAYOUT_HELPERS
#ifdef HELTEC_LORA_V4
 const char* samples[]={"Sleep: yes","Sleep: n/a","LNA: off","Повт: выкл",
 "Русское длинное имя получателя","Very long label with 1234567890", "100%"};
 for(auto left:samples) for(auto right:samples) {
  display.draws.clear(); drawColumnLeft(0,18,left); drawColumnRight(127,18,right);
  assert(display.draws.size()==2);
  auto l=display.draws[0],r=display.draws[1];
  assert(l.x>=3 && l.x+l.w<=63 && r.x>=65 && r.x+r.w<=125);
 }
 for(auto label:samples) for(int row=0;row<4;row++) {
  display.draws.clear(); drawLeft(0,18+11*row,label);
  auto d=display.draws.back();
  assert(d.x>=3 && d.x+d.w<=125 && d.y-1>=13 && d.y+8<=59);
 }
 display.draws.clear(); drawClipped(36,29,33,samples[4]);drawClipped(72,29,53,samples[4]);
 assert(display.draws[0].x+display.draws[0].w<=69);
 assert(display.draws[1].x>=72 && display.draws[1].x+display.draws[1].w<=125);
#else
 display.draws.clear();drawColumnLeft(0,18,"abc");drawColumnRight(127,18,"abc");
 assert(display.draws[0].x==0 && display.draws[1].x==109);
#endif
}
int main(){
 checkLayout();
 Home h; h.returnToMain();
 assert(h._page==Home::FIRST && h._sub_depth==0 && h.stopped==1);
 assert(!h._act_open && !h._settings_action_page && !h._radio_details && !h._radio_confirm
 && !h._scan_confirm && !h._conn_confirm && !h._power_confirm && !h._setup_edit
 && !h._adc_edit && !h._battery_confirm && !h._shutdown_init && !h._shutdown_apply);
 for(int n=1;n<=4;n++) {
  MomentaryButton btn(1,800,false,false,true);
  for(int i=0;i<n;i++) {
   clock_ms+=80;button_level=HIGH;assert(btn.check()==BUTTON_EVENT_NONE);
   clock_ms+=60;button_level=LOW;assert(btn.check()==BUTTON_EVENT_NONE);
  }
  clock_ms+=300;
  int expected[]={0,BUTTON_EVENT_CLICK,BUTTON_EVENT_DOUBLE_CLICK,BUTTON_EVENT_TRIPLE_CLICK,BUTTON_EVENT_QUAD_CLICK};
  assert(btn.check()==expected[n]); assert(btn.check()==BUTTON_EVENT_NONE);
 }
 puts("PASS layout bounds, Cyrillic, home reset and 1/2/3/4 clicks");
}
'''.replace('LAYOUT_HELPERS',layout).replace('RESET_METHOD',reset)
with tempfile.TemporaryDirectory() as tmp:
    d=Path(tmp)
    (d/'Arduino.h').write_text(mock)
    (d/'check.cpp').write_text(source)
    for flags in (['-DHELTEC_LORA_V4'],[]):
        subprocess.run(['g++','-std=c++11','-fsanitize=undefined','-I'+tmp,
            '-I'+str(root/'src'),*flags,str(d/'check.cpp'),
            str(root/'src/helpers/ui/MomentaryButton.cpp'),'-o',str(d/'check')],check=True)
        subprocess.run([str(d/'check')],check=True)
# Both visual history sampling and render scheduling must use 500ms on V4.
assert '_nf_sample_at = millis() + (padded ? 500 : 2000)' in ui
assert 'HomePage::RADIO && compact_header) return padded ? 500 : 2000' in ui
ini = (root / 'variants/heltec_v4/platformio.ini').read_text()
section = ini.split('[env:heltec_v4_oled_companion_usb_ble_wifi]', 1)[1].split('\n[', 1)[0]
assert '-D NOISE_FLOOR_CALIB_INTERVAL=500' in section
print('PASS V4 NF history, render and target calibration interval 500 ms')
