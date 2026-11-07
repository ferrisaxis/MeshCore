#include "UITask.h"
#include <helpers/RadioProfiles.h>
#include <helpers/ui/BatteryLevel.h>
#include <helpers/ui/ClockFont.h>
#include <helpers/ui/Localize.h>
#include <math.h>
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#include <time.h>
#ifdef ENABLE_WIFI_INTERFACE
  #include <WiFi.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

// Used both for recent adverts and discovered nodes
#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif
#define NEIGHBOR_SCROLL_MS  2000

#define SCAN_SAMPLE_MILLIS    50   // noise scan RSSI sampling, independent of the frame rate
#ifndef SCAN_REFRESH_MILLIS
  #define SCAN_REFRESH_MILLIS       200
#endif
#ifndef SCAN_REFRESH_MILLIS_EINK
  #define SCAN_REFRESH_MILLIS_EINK 3000   // each e-ink update blocks the loop
#endif
#define FSCAN_REFRESH_MILLIS  250   // progress bar while the freq scan runs

#ifndef MSG_PAGE_MILLIS
  #if AUTO_OFF_MILLIS==0   // e-ink: every page flip is a full refresh
    #define MSG_PAGE_MILLIS 10000
  #else
    #define MSG_PAGE_MILLIS  4000
  #endif
#endif

#ifndef FSCAN_MAX
  #define FSCAN_MAX 17
#endif

#if UI_HAS_JOYSTICK || UI_HAS_ROTARY_INPUT
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"

struct PowerChoice {
  bool boost;
  bool lna;
  bool cpu;
  const char* name;
};

// Names run from most economical to least. Missing options (no FEM, no CPU
// sleep) are skipped, so Eco max is always the cheapest mode on this board.
static const char* savingsName(int index, int count) {
  static const char* labels[] = {"Eco max", "Eco high", "Eco good", "Eco fair", "Eco low", "Eco min"};
  if (count <= 1) return labels[0];
  int slot = index * 5 / (count - 1);
  if (slot < 0) slot = 0;
  if (slot > 5) slot = 5;
  return labels[slot];
}

static int buildPowerChoices(bool fem, bool cpu_ok, PowerChoice* out) {
  int n = 0;
  if (cpu_ok) {
    out[n++] = {false, false, true, NULL};
    out[n++] = {true, false, true, NULL};
    if (fem) out[n++] = {true, true, true, NULL};
  }
  out[n++] = {false, false, false, NULL};
  out[n++] = {true, false, false, NULL};
  if (fem) out[n++] = {true, true, false, NULL};
  for (int i = 0; i < n; i++) out[i].name = savingsName(i, n);
  return n;
}

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[24];

public:
  SplashScreen(UITask* task) : _task(task) {
    firmwareVersionNoHash(_version_info, sizeof(_version_info), FIRMWARE_VERSION);

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    // meshcore logo
    display.setColor(UIColor::corp_blue);
    int logoWidth = 128;
    display.drawXbm((display.width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // meshcore website
    const char* website = "https://meshcore.io";
    display.setColor(UIColor::primary_txt);
    display.setTextSize(1);
    uint16_t websiteWidth = display.getTextWidth(website);
    display.setCursor((display.width() - websiteWidth) / 2, 22);
    display.print(website);

    // version info
    display.setColor(UIColor::primary_txt);
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 35, _version_info);

    display.setColor(UIColor::secondary_txt);
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 48, "by FDSKY");

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

static void clockYmdFromUnix(uint32_t unix_s, int& year, int& month, int& day, int& wday) {
  uint32_t days = unix_s / 86400UL;
  wday = (int)((days + 4) % 7);  // 1970-01-01 was Thursday; 0 = Sunday
  year = 1970;
  for (;;) {
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    uint32_t diy = leap ? 366 : 365;
    if (days < diy) break;
    days -= diy;
    year++;
  }
  static const uint8_t mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  month = 1;
  for (int i = 0; i < 12; i++) {
    int dim = mdays[i];
    if (i == 1 && leap) dim = 29;
    if (days < (uint32_t)dim) {
      day = (int)days + 1;
      return;
    }
    days -= dim;
    month++;
  }
  day = 1;
}

static void formatClockHm(uint32_t t, int16_t tz_mins, char* buf, size_t n) {
  int64_t local = (int64_t)t + (int64_t)tz_mins * 60;
  if (local < 0) local = 0;
  uint32_t mins = ((uint32_t)local / 60UL) % (24UL * 60UL);
  snprintf(buf, n, "%02u:%02u", (unsigned)(mins / 60UL), (unsigned)(mins % 60UL));
}

static void formatClockDate(uint32_t t, int16_t tz_mins, uint8_t lang, char* buf, size_t n) {
  int64_t local = (int64_t)t + (int64_t)tz_mins * 60;
  if (local < 0) local = 0;
  int year, month, day, wday;
  clockYmdFromUnix((uint32_t)local, year, month, day, wday);
  static const char* en[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* ru[] = {"Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"};
  const char* name = (lang == UI_LANG_EN) ? en[wday] : ru[wday];
  snprintf(buf, n, "%s, %02d.%02d", name, day, month);
  (void)year;
}

#ifdef HELTEC_LORA_V4
static constexpr int OLED_INSET = 3;
static constexpr int OLED_HEADER_HEIGHT = 16;
static constexpr int OLED_HEADER_TEXT_Y = 4;  // Cyrillic cell starts one pixel above the cursor

static void drawV4ListRow(DisplayDriver& display, int row, bool selected, const char* text, const char* right = "") {
  const int y = 18 + row * 11;
  if (selected) {
    display.setColor(UIColor::primary_txt);
    display.fillRect(0, y - 1, display.width(), 11);
  }
  display.setColor(selected ? UIColor::window_bkg : UIColor::primary_txt);
  int right_width = display.getTextWidth(right);
  if (right_width > 0) {
    display.drawTextRightAlign(display.width() - 4, y, right);
    right_width += 4;
  }
  char label[64];
  display.translateUTF8ToBlocks(label, text, sizeof(label));
  display.drawTextEllipsized(4, y, display.width() - 8 - right_width, label);
}
#endif

class HomeScreen : public UIScreen {
public:
  enum HomePage {
    FIRST,
    MESSAGES,      // root: message counters, opens the preview
    BLUETOOTH,     // root: interface status (_("Links"))
    CONNECTIONS,   // sub-screen of BLUETOOTH: toggle the transports
    RECENT,
    NEIGHBORS,     // root: network (_("Network"))
    RADIO,
    POWER,
#ifdef HELTEC_LORA_V4
    BATTERY,
    BATTERY_CAL,
    LANG,
#endif
    SCAN,
    FSCAN,
#if ENV_INCLUDE_GPS == 1
    GPS,
    BEACON,
#ifdef HELTEC_LORA_V4
    BEACON_MODE,
    BEACON_RECIPIENT,
    BEACON_INTERVAL,
    BEACON_STOP,
#endif
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
    TIME,          // sub-screen: clock, date, timezone, clock pull
    CLOCK_NODES,   // sub-screen: trusted nodes to pull the clock from
    NODE_PICKER,   // sub-screen: pick a contact to trust for time
    SETUP,
    ABOUT,         // sub-screen: version, build date, node identity
    MENU,          // root: entry point for every settings sub-screen
    SHUTDOWN,
    Count    // keep as last
  };

private:

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  uint8_t _setup_row;
  bool _setup_edit;
  bool _shutdown_init;
  bool _shutdown_apply = false;   // confirm dialog: false = Отмена (default)
#ifdef HELTEC_LORA_V4
  uint8_t _battery_choice = 0; // full charge, reset, CLI instructions
  bool _battery_confirm = false;
  bool _battery_apply = false;
  bool _adc_edit = false;        // multimeter reference editor
  uint16_t _adc_mv = 4200;       // reference voltage being entered
  uint8_t _adc_digit = 0;        // thousands, hundreds, tens, ones
  uint8_t _lang_sel = UI_LANG_EN;
#endif
  uint8_t _conn_selected = 0;
  // One entry per nested screen; a double click always returns one level.
  uint8_t _sub_stack[4] = {};
  uint8_t _sub_depth = 0;
  uint8_t _menu_index = 0;
  uint8_t _menu_scroll = 0;
  bool _radio_details = false;
  bool _radio_confirm = false;
  bool _radio_apply = false;
#ifdef HELTEC_LORA_V4
  bool _scan_confirm = false;
  bool _scan_apply = false;
  bool _settings_action_page = false;
#if ENV_INCLUDE_GPS == 1
  MyMesh::BeaconOpt _beacon_options[MyMesh::BEACON_OPT_MAX];
  int _beacon_count = 0;
  int _beacon_sel = 0;
  uint8_t _beacon_for_mode = BEACON_OFF;
#endif
  bool _conn_confirm = false;
  bool _conn_apply = false;
  bool _conn_was_enabled = false;
  bool _power_confirm = false;
  bool _power_apply = false;
  bool _power_boost = false, _power_lna = false, _power_cpu = false;
  const char* _power_name = "";
#endif
  uint8_t _radio_target = RADIO_PROFILE_NORMAL;
  int16_t _nf_history[12] = {};
  uint8_t _nf_count = 0;
  uint8_t _nf_next = 0;
  unsigned long _nf_sample_at = 0;
  unsigned long _nf_valid_at = 0;
  float _nf_freq = 0, _nf_bw = 0;
  uint8_t _nf_sf = 0, _nf_cr = 0;

public:
  // Accessors for UITask
  uint8_t getPage() const { return _page; }
  uint8_t getConnSelected() const { return _conn_selected; }
  void setConnSelected(uint8_t v) { _conn_selected = v; }
  // Navigation from outside (e.g., triple-click on CONNECTIONS)
  void stepPageNav(int dir) { _page = stepPage(dir); }
#ifdef HELTEC_LORA_V4
  void returnToMain() {
    stopFreqScan();
    _page = HomePage::FIRST;
    _sub_depth = 0;
    _act_open = _settings_action_page = false;
    _radio_details = _radio_confirm = _scan_confirm = _conn_confirm = _power_confirm = false;
    _setup_edit = _adc_edit = _battery_confirm = false;
    _shutdown_init = _shutdown_apply = false;
  }
#endif
  // Jump straight to a sub-screen, bypassing the root flip cycle
  void enterSub(uint8_t p) {
    if (_sub_depth >= sizeof(_sub_stack)) return;
    _sub_stack[_sub_depth++] = _page;
    _act_open = false;
    _page = p;
    if (p == HomePage::RADIO) _radio_details = false;
  }
  void leaveSub() {
    if (_sub_depth) _page = _sub_stack[--_sub_depth];
    _act_open = false;
  }
  void goToPage(uint8_t p) { enterSub(p); }
  bool isInSub() const { return _sub_depth != 0; }
  uint8_t getMenuIndex() const { return _menu_index; }
  void setMenuIndex(uint8_t v) { _menu_index = v; }

private:
  AdvertPath recent[UI_RECENT_LIST_SIZE];
  NeighborInfo neighbor_list[NEIGHBOR_TABLE_SIZE];
#if UI_DISCOVER_SCREEN
  DiscoveredNode discovered[DISCOVERED_NODES_TABLE_SIZE]; // not circular, latest discovered nodes are not kept
  uint32_t disc_node_req_tag = 0;
  uint32_t disc_nodes_count = 0;
  uint32_t discovery_req_time = 0;
  bool discovery_disp_names = true; // by default display names if available (removes SNR_O)
#endif
  int neighbors_scroll_offset = 0;
  unsigned long neighbors_next_scroll = 0;
  int16_t scan_peak;
  int16_t scan_live = -127;
  int32_t scan_sum = 0;       // RSSI samples collected since the last frame
  uint16_t scan_cnt = 0;
  bool scan_pkt = false;
  unsigned long scan_next_sample = 0;

  void sampleNoise() {
    float rssi = radio_driver.getCurrentRSSI();
    int v = (int)(rssi + (rssi < 0 ? -0.5f : 0.5f));
    if (radio_driver.isReceivingPacket()) {
      scan_pkt = true;
      return;
    }
    scan_sum += v;
    scan_cnt++;
    if (scan_peak < -120 || v > scan_peak) scan_peak = (int16_t)v;
  }

  struct FreqScanBin {
    float freq;
    int16_t rssi;
  };
  enum { FSCAN_IDLE, FSCAN_RUN, FSCAN_DONE };
  uint8_t fscan_state;
  uint8_t fscan_i;
  uint8_t fscan_n;
  unsigned long fscan_ready_at;
  FreqScanBin fscan[FSCAN_MAX];
  FreqScanBin fscan_best[3];

  static int cmpFreqScan(const void* a, const void* b) {
    return ((const FreqScanBin*)a)->rssi - ((const FreqScanBin*)b)->rssi;
  }

  void freqScanBand(float center, float& lo, float& hi) {
    if (center >= 850.0f && center < 890.0f) { lo = 863.0f; hi = 870.0f; }
    else if (center >= 900.0f && center < 932.0f) { lo = 902.0f; hi = 928.0f; }
    else { lo = center - 1.0f; hi = center + 1.0f; }
  }

  void applyScanFreq(float f) {
    radio_driver.setFrequency(f);
  }

  void stopFreqScan() {
    if (fscan_state != FSCAN_RUN) return;
    if (fscan_i > 0) {
      fscan_n = fscan_i;
      finishFreqScan();
    } else {
      the_mesh.restoreRadioAfterScan();
      fscan_state = FSCAN_IDLE;
    }
  }

  void startFreqScan() {
    float step = _node_prefs->bw / 1000.0f;
    if (step < 0.025f) step = 0.025f;
    float center = _node_prefs->freq;
    float lo, hi;
    freqScanBand(center, lo, hi);

    fscan_n = 0;
    fscan[fscan_n++].freq = center;
    for (int k = 1; fscan_n < FSCAN_MAX; k++) {
      float down = center - (float)k * step;
      float up = center + (float)k * step;
      bool added = false;
      if (down >= lo - 0.0001f) {
        fscan[fscan_n++].freq = down;
        added = true;
        if (fscan_n >= FSCAN_MAX) break;
      }
      if (up <= hi + 0.0001f) {
        fscan[fscan_n++].freq = up;
        added = true;
      }
      if (!added) break;
    }
    for (int i = 0; i < fscan_n; i++) fscan[i].rssi = 0;
    fscan_i = 0;
    fscan_state = FSCAN_RUN;
    radio_driver.pauseNoiseFloor(true);
    applyScanFreq(fscan[0].freq);
    fscan_ready_at = millis() + 35;
  }

  void finishFreqScan() {
    the_mesh.restoreRadioAfterScan();
    FreqScanBin sorted[FSCAN_MAX];
    memcpy(sorted, fscan, fscan_n * sizeof(FreqScanBin));
    qsort(sorted, fscan_n, sizeof(FreqScanBin), cmpFreqScan);
    int out = 0;
    for (int i = 0; i < fscan_n && out < 3; i++) {
      fscan_best[out++] = sorted[i];
    }
    fscan_state = FSCAN_DONE;
  }

  void stepFreqScan() {
    if (millis() < fscan_ready_at || fscan_i >= fscan_n) return;
    int sum = 0, n = 0;
    for (int s = 0; s < 8; s++) {
      if (!radio_driver.isReceivingPacket()) {
        sum += (int)radio_driver.getCurrentRSSI();
        n++;
      }
      delay(2);
    }
    fscan[fscan_i].rssi = (int16_t)(n > 0 ? (sum / n) : (int)radio_driver.getCurrentRSSI());
    fscan_i++;
    if (fscan_i >= fscan_n) {
      finishFreqScan();
      _task->requestRefresh();
    } else {
      applyScanFreq(fscan[fscan_i].freq);
      fscan_ready_at = millis() + 35;
    }
  }



public:
  static void renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts, bool inverted,
                                     bool buzzer_quiet, int right, int y) {
    // Display a smoothed one-percent estimate; ADC calibration corrects the
    // voltage first. This is a voltage estimate, not a capacity measurement.
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif
    const int minMilliVolts = BATT_MIN_MILLIVOLTS;
    const int maxMilliVolts = BATT_MAX_MILLIVOLTS;
    static uint16_t samples[5] = {};
    static uint8_t count = 0, index = 0;
    static int displayed = -1;
    display.setColor(inverted ? UIColor::window_bkg : UIColor::title_txt);
    if (batteryMilliVolts < 2000 || maxMilliVolts <= minMilliVolts) {
      displayed = -1;
      count = index = 0;
      display.drawTextRightAlign(right, y, "--%");
      return;
    }
    samples[index] = batteryMilliVolts;
    index = (index + 1) % 5;
    if (count < 5) count++;
    uint16_t ordered[5];
    for (int i = 0; i < count; i++) ordered[i] = samples[i];
    for (int i = 1; i < count; i++) {
      uint16_t sample = ordered[i];
      int j = i;
      while (j > 0 && ordered[j - 1] > sample) { ordered[j] = ordered[j - 1]; j--; }
      ordered[j] = sample;
    }
    int median = ordered[count / 2];
    // 1S Li-ion/LiPo discharge curve. A straight 3000..4200 mV line badly
    // under-reports the long plateau around 3.7..3.9 V, so interpolate a table
    // whenever the build keeps the stock range; custom ranges stay linear.
    static const uint16_t lipo_mv[] = {3300, 3500, 3650, 3740, 3800, 3870,
                                       3950, 4030, 4100, 4170, 4200};
    static const uint8_t lipo_pc[] = {   0,    5,   15,   30,   45,   60,
                                        72,   82,   90,   97,  100};
    int pct;
    if (minMilliVolts == 3000 && maxMilliVolts == 4200) {
      const int n = (int)(sizeof(lipo_pc) / sizeof(lipo_pc[0]));
      pct = 0;
      if (median >= lipo_mv[n - 1]) {
        pct = 100;
      } else if (median > lipo_mv[0]) {
        for (int i = 1; i < n; i++) {
          if (median <= lipo_mv[i]) {
            int span_mv = lipo_mv[i] - lipo_mv[i - 1];
            int span_pc = lipo_pc[i] - lipo_pc[i - 1];
            pct = lipo_pc[i - 1] + (median - lipo_mv[i - 1]) * span_pc / span_mv;
            break;
          }
        }
      }
    } else {
      pct = (median - minMilliVolts) * 100 / (maxMilliVolts - minMilliVolts);
    }
    pct = constrain(pct, 0, 100);
    if (displayed < 0 || abs(pct - displayed) >= 2) displayed = pct;
    char label[6];
    snprintf(label, sizeof(label), "%d%%", displayed);
    display.drawTextRightAlign(right, y, label);
#ifdef PIN_BUZZER
    if (buzzer_quiet) {
      display.setColor(inverted ? UIColor::window_bkg : UIColor::warning_txt);
      display.drawXbm(display.width() == 128 && !display.isEink() ? 0 : display.width() - 37,
                      2, muted_icon, 8, 8);
    }
#else
    (void)buzzer_quiet;
#endif
  }

#ifdef HELTEC_LORA_V4
  static void renderV4Header(DisplayDriver& display, UITask& task, const char* title, bool nested,
                             const char* status = "") {
    display.setTextSize(1);
    display.setColor(UIColor::primary_txt);
    display.fillRect(0, 0, display.width(), OLED_HEADER_HEIGHT);
    display.setColor(UIColor::window_bkg);

    const int right = display.width() - OLED_INSET;
    int title_end = right - display.getTextWidth("100%") - OLED_INSET;
    if (status[0]) {
      display.drawTextRightAlign(title_end, OLED_HEADER_TEXT_Y, status);
      title_end -= display.getTextWidth(status) + OLED_INSET;
    }

    int x = OLED_INSET;
    if (nested) {
      display.drawTextLeftAlign(x, OLED_HEADER_TEXT_Y, "./");
      x += display.getTextWidth("./");
    }
    char shown_title[40];
    display.translateUTF8ToBlocks(shown_title, title, sizeof(shown_title));
    display.drawTextEllipsized(x, OLED_HEADER_TEXT_Y, title_end - x, shown_title);
    renderBatteryIndicator(display, task.getBattMilliVolts(), true, task.isBuzzerQuiet(), right, OLED_HEADER_TEXT_Y);
  }
#endif

private:
  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;
  static const int UI_SENSORS_ROWS = 3;   // 3 rows of 12 px fit under the header

  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);
      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
#if AUTO_OFF_MILLIS > 0
      next_sensors_refresh = millis() + 5000; // refresh sensor values every 5 sec
#else
      next_sensors_refresh = millis() + 60000; // refresh sensor values every 1 min
#endif
    }
  }

  // The gps setting is published only after the module is detected.
  // Its value is "1" only while that module is switched on.
  // Only five root screens take part in the click / double-click cycle.
  // Every other page is a sub-screen reached from МЕНЮ (or СВЯЗЬ from Связь)
  // and left with a double click.
  bool isPageVisible(uint8_t page) const {
    switch (page) {
      case HomePage::FIRST:
      case HomePage::MESSAGES:
      case HomePage::BLUETOOTH:
      case HomePage::NEIGHBORS:
#ifdef HELTEC_LORA_V4
      case HomePage::RADIO:
      case HomePage::FSCAN:
      case HomePage::RECENT:
      case HomePage::SCAN:
#if ENV_INCLUDE_GPS == 1
      case HomePage::GPS:
      case HomePage::BEACON:
#endif
#if UI_SENSORS_PAGE == 1
      case HomePage::SENSORS:
#endif
#endif
      case HomePage::MENU:
        return true;
      default:
        return false;
    }
  }

  // Entries of the МЕНЮ list, in display order. Conditional ones are dropped
  // when the build or the hardware does not provide them.
  uint8_t menuCount() const {
#ifdef HELTEC_LORA_V4
    uint8_t n = 8;  // radio, links, power, battery, display, time, language, shutdown
#if ENV_INCLUDE_GPS == 1
    n += 2;  // GPS, beacon
#endif
    return n + 1;  // firmware information is always last
#else
    uint8_t n = 0;
    n++;  // Радио
    n++;  // Питание
#ifdef HELTEC_LORA_V4
    n++;  // Батарея
#endif
    n++;  // Шум
    n++;  // Частоты
    n++;  // Недавние
#if UI_SENSORS_PAGE == 1
    n++;  // Датчики
#endif
#if ENV_INCLUDE_GPS == 1
    n++;  // GPS
    n++;  // Маяк
#endif
    n++;  // Время
    n++;  // Экран
    n++;  // О прошивке
    n++;  // Выключить
    return n;
#endif
  }

  uint8_t menuPageAt(uint8_t idx) const {
    uint8_t i = 0;
#ifdef HELTEC_LORA_V4
    if (idx == i++) return HomePage::RADIO;
    if (idx == i++) return HomePage::CONNECTIONS;
    if (idx == i++) return HomePage::POWER;
    if (idx == i++) return HomePage::BATTERY;
#if ENV_INCLUDE_GPS == 1
    if (idx == i++) return HomePage::GPS;
    if (idx == i++) return HomePage::BEACON;
#endif
    if (idx == i++) return HomePage::SETUP;
    if (idx == i++) return HomePage::TIME;
    if (idx == i++) return HomePage::LANG;
    if (idx == i++) return HomePage::SHUTDOWN;
    return HomePage::ABOUT;
#else
    if (idx == i++) return HomePage::RADIO;
    if (idx == i++) return HomePage::POWER;
#ifdef HELTEC_LORA_V4
    if (idx == i++) return HomePage::BATTERY;
#endif
    if (idx == i++) return HomePage::SCAN;
    if (idx == i++) return HomePage::FSCAN;
    if (idx == i++) return HomePage::RECENT;
#if UI_SENSORS_PAGE == 1
    if (idx == i++) return HomePage::SENSORS;
#endif
#if ENV_INCLUDE_GPS == 1
    if (idx == i++) return HomePage::GPS;
    if (idx == i++) return HomePage::BEACON;
#endif
    if (idx == i++) return HomePage::TIME;
    if (idx == i++) return HomePage::SETUP;
    if (idx == i++) return HomePage::ABOUT;
    return HomePage::SHUTDOWN;
#endif
  }

  const char* pageTitle() const {
    switch (_page) {
      case FIRST: return _node_prefs->node_name;
      case MESSAGES: return _("Messages");
      case RECENT: return _("Recent");
      case NEIGHBORS: return _("Network");
      case RADIO: return _("Radio");
      case POWER: return _("Power");
#ifdef HELTEC_LORA_V4
      case BATTERY: case BATTERY_CAL: return _("Battery");
      case LANG: return _("Language");
#endif
      case SCAN: return _("Noise");
      case FSCAN: return _("Freqs");
      case BLUETOOTH: return _("Links");
      case CONNECTIONS: return _("Interfaces");
#if ENV_INCLUDE_GPS == 1
      case GPS: return "GPS";
      case BEACON: return _("Beacon");
#ifdef HELTEC_LORA_V4
      case BEACON_MODE: return _("Beacon mode");
      case BEACON_RECIPIENT: return _("Recipient");
      case BEACON_INTERVAL: return _("Interval");
      case BEACON_STOP: return _("Beacon");
#endif
#endif
#if UI_SENSORS_PAGE == 1
      case SENSORS: return _("Sensors");
#endif
      case TIME: return _("Time");
      case CLOCK_NODES: return _("Nodes");
      case NODE_PICKER: return _("Pick node");
      case SETUP: return _("Display");
      case ABOUT:
#ifdef HELTEC_LORA_V4
        return _("Information");
#else
        return _("Firmware");
#endif
      case MENU:
#ifdef HELTEC_LORA_V4
        return _("Settings");
#else
        return _("Menu");
#endif
      case SHUTDOWN: return _("Power off");
      default: return "MeshCore";
    }
  }

  // Short names for the МЕНЮ list; keep them under 19 characters.
  const char* menuLabel(uint8_t page) const {
    switch (page) {
      case HomePage::RADIO: return _("Radio");
      case HomePage::POWER: return _("Power");
      case HomePage::CONNECTIONS: return _("Interfaces");
#ifdef HELTEC_LORA_V4
      case HomePage::BATTERY: return _("Battery");
      case HomePage::LANG: return _("Language");
#endif
      case HomePage::SCAN: return _("Noise");
      case HomePage::FSCAN: return _("Freqs");
      case HomePage::RECENT: return _("Recent");
#if UI_SENSORS_PAGE == 1
      case HomePage::SENSORS: return _("Sensors");
#endif
#if ENV_INCLUDE_GPS == 1
      case HomePage::GPS: return _task->getGPSState() ? _("GPS: on") : _("GPS: off");
      case HomePage::BEACON: return _("Beacon");
#endif
      case HomePage::TIME: return _("Time");
      case HomePage::SETUP: return _("Display");
      case HomePage::ABOUT:
#ifdef HELTEC_LORA_V4
        return _("Information");
#else
        return _("Firmware");
#endif
      case HomePage::SHUTDOWN: return _("Power off");
      default: return "?";
    }
  }

  // Auto-off presets, index 0 = never. Mirrored in NodePrefs::screen_off_idx.
  static const uint16_t screenOffSeconds(uint8_t idx) {
    static const uint16_t secs[5] = {0, 15, 30, 60, 120};
    return secs[idx < 5 ? idx : 3];
  }

  // ---- action menu: a hold on a sub-screen opens the list of its actions,
  // then click moves the marker and a second hold runs the highlighted one.
  bool _act_open = false;
  uint8_t _act_idx = 0;
  uint32_t _pick_idx = 0;   // highlighted contact in NODE_PICKER

  int usedClockNodes() const {
    if (!_node_prefs) return 0;
    int n = 0;
    for (int i = 0; i < CLOCK_NODE_MAX; i++) {
      if (clockNodeSlotUsed(_node_prefs->clock_nodes[i])) n++;
    }
    return n;
  }

  void addClockNodeFromPicker() {
    if (!_node_prefs) return;
    ContactInfo c;
    if (!the_mesh.getContactByIdx(_pick_idx, c)) {
      _task->showAlert(_("no contact"), 1200);
      return;
    }
    int rc = clockNodeAdd(_node_prefs->clock_nodes, c.id.pub_key);
    if (rc == 1) {
      the_mesh.savePrefs();
      _task->notify(UIEventType::ack);
      _task->showAlert(_("node added"), 1200);
    } else if (rc == 0) {
      _task->showAlert(_("already a node"), 1200);
    } else {
      _task->showAlert(_("list is full"), 1200);
    }
  }

  void requestClock() {
    bool ok = the_mesh.pullClock();
    _task->showAlert(ok ? _("time request") : _("no one to ask"), 1200);
  }

#if defined(HELTEC_LORA_V4) && ENV_INCLUDE_GPS == 1
  bool isBeaconPicker() const {
    return _page >= HomePage::BEACON_MODE && _page <= HomePage::BEACON_STOP;
  }

  void openBeaconRecipient(uint8_t for_mode) {
    _beacon_for_mode = for_mode;
    uint8_t filter = for_mode;
    if (filter == BEACON_OFF && _node_prefs->beacon_mode >= BEACON_GROUP) {
      filter = _node_prefs->beacon_mode;
    }
    int count = the_mesh.buildBeaconOptions(_beacon_options, MyMesh::BEACON_OPT_MAX);
    _beacon_count = 0;
    for (int i = 0; i < count; i++) {
      const auto& opt = _beacon_options[i];
      if (opt.mode >= BEACON_GROUP && (filter == BEACON_OFF || opt.mode == filter)) {
        _beacon_options[_beacon_count++] = opt;
      }
    }
    _beacon_sel = 0;
    enterSub(HomePage::BEACON_RECIPIENT);
  }

  int beaconPickerCount() const {
    if (_page == HomePage::BEACON_MODE) return BEACON_CHAT + 1;
    if (_page == HomePage::BEACON_RECIPIENT) return _beacon_count;
    if (_page == HomePage::BEACON_STOP) return 2;
    return _node_prefs->beacon_mode >= BEACON_GROUP ? MyMesh::beaconIntervalCount() : 0;
  }

  void closeBeaconPicker() {
    uint8_t page = _page;
    leaveSub();
    if (_page == HomePage::BEACON_MODE) {
      _beacon_sel = _beacon_for_mode;
    } else {
      _act_open = true;
      if (page == HomePage::BEACON_MODE) _act_idx = 0;
      else if (page == HomePage::BEACON_RECIPIENT) _act_idx = 1;
      else if (page == HomePage::BEACON_INTERVAL) _act_idx = 2;
      else _act_idx = 3;
    }
  }

  void renderBeaconPicker(DisplayDriver& display) {
    display.setColor(UIColor::primary_txt);
    int count = beaconPickerCount();
    if (_page == HomePage::BEACON_RECIPIENT && count == 0) {
      display.drawTextLeftAlign(4, 18, _("No recipient"));
      display.drawTextLeftAlign(4, 30, _("Add a contact"));
      display.drawTextLeftAlign(4, 42, _("or channel in app"));
      return;
    }
    if (_page == HomePage::BEACON_INTERVAL && count == 0) {
      if (_node_prefs->beacon_mode != BEACON_OFF) {
        char interval[24];
        snprintf(interval, sizeof(interval), _("%u min"), (unsigned)the_mesh.beaconMinutes());
        display.drawTextLeftAlign(4, 18, interval);
        display.drawTextLeftAlign(4, 30, _("Fixed for adverts"));
      } else {
        display.drawTextLeftAlign(4, 18, _("Choose beacon mode"));
      }
      return;
    }
    if (_page == HomePage::BEACON_STOP) {
      display.drawTextLeftAlign(4, 18, _("Stop beacon?"));
      drawV4ListRow(display, 1, _beacon_sel == 0, _("Cancel"));
      drawV4ListRow(display, 2, _beacon_sel == 1, _("Stop"));
      return;
    }

    const char* modes[] = {_("Off"), _("Local advert"), _("Flood advert"), _("Group"), _("Direct message")};
    int first = _beacon_sel >= 4 ? _beacon_sel - 3 : 0;
    for (int i = first; i < count && i < first + 4; i++) {
      char label[36];
      const char* name = label;
      bool saved = false;
      if (_page == HomePage::BEACON_MODE) {
        name = modes[i];
        saved = i == _node_prefs->beacon_mode;
      } else if (_page == HomePage::BEACON_RECIPIENT) {
        ChannelDetails channel;
        const auto& opt = _beacon_options[i];
        const char* recipient = the_mesh.beaconRecipientName(opt, channel);
        if (recipient) {
          snprintf(label, sizeof(label), "%c %s", opt.mode == BEACON_GROUP ? '#' : '@', recipient);
          saved = the_mesh.isSavedBeaconRecipient(opt);
        } else {
          name = _("Unavailable");
        }
      } else {
        unsigned minutes = MyMesh::beaconIntervalAt(i);
        snprintf(label, sizeof(label), _("%u min"), minutes);
        saved = minutes == the_mesh.beaconMinutes();
      }
      drawV4ListRow(display, i - first, i == _beacon_sel, name, saved ? "*" : "");
    }
  }

  void handleBeaconPicker(char c) {
    int count = beaconPickerCount();
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      if (_beacon_sel + 1 < count) _beacon_sel++;
    } else if (c == KEY_PREV || c == KEY_LEFT) {
      if (_beacon_sel > 0) _beacon_sel--;
      else closeBeaconPicker();
    } else if (c == KEY_ENTER && _beacon_sel < count) {
      if (_page == HomePage::BEACON_MODE) {
        if (_beacon_sel >= BEACON_GROUP) {
          openBeaconRecipient(_beacon_sel);
          return;
        }
        the_mesh.setBeaconAdvertMode(_beacon_sel);
      } else if (_page == HomePage::BEACON_RECIPIENT) {
        if (!the_mesh.selectBeaconRecipient(_beacon_options[_beacon_sel], _beacon_for_mode != BEACON_OFF)) {
          _task->showAlert(_("Unavailable"), 1200);
          return;
        }
        if (_beacon_for_mode != BEACON_OFF) leaveSub();  // skip the completed mode picker
      } else if (_page == HomePage::BEACON_INTERVAL) {
        the_mesh.setBeaconInterval(MyMesh::beaconIntervalAt(_beacon_sel));
      } else if (_beacon_sel != 0) {
        the_mesh.stopBeacon();
      }
      closeBeaconPicker();
      _task->notify(UIEventType::ack);
    }
  }
#endif

  uint8_t actionCount() const {
    switch (_page) {
      case HomePage::RADIO:  return _task->canControlFemLna() ? 3 : 2;
      case HomePage::POWER:  return 2;
      case HomePage::TIME:        return 2;
      case HomePage::CLOCK_NODES: return 3;
      case HomePage::NODE_PICKER: return 1;
#if ENV_INCLUDE_GPS == 1
      case HomePage::BEACON:
#ifdef HELTEC_LORA_V4
        return 4;
#else
        return 3;
#endif
#ifdef HELTEC_LORA_V4
      case HomePage::GPS: return 1;
#endif
#endif
      default: return 0;
    }
  }

  const char* actionLabel(uint8_t i) const {
    switch (_page) {
      case HomePage::RADIO:
        if (i == 0) return _("Profile: next");
        if (i == 1) return (_node_prefs && _node_prefs->rx_boosted_gain) ? _("RX Boost: off") : _("RX Boost: on");
        return (_node_prefs && _node_prefs->radio_fem_rxgain) ? _("LNA: off") : _("LNA: on");
      case HomePage::POWER:
        return i == 0 ? _("Profile: next") : _("Profile: back");
      case HomePage::TIME:
        return i == 0 ? _("Request time") : _("Time nodes");
      case HomePage::CLOCK_NODES:
        if (i == 0) return _("Add node");
        if (i == 1) return _("Request time");
        return _("Clear all");
      case HomePage::NODE_PICKER:
        return _("Add as node");
#if ENV_INCLUDE_GPS == 1
      case HomePage::GPS:
#ifdef HELTEC_LORA_V4
        return _task->getGPSState() ? _("GPS: off") : _("GPS: on");
#else
        return "";
#endif
      case HomePage::BEACON:
#ifdef HELTEC_LORA_V4
        if (i == 0) return _("Mode");
        if (i == 1) return _("Recipient");
        if (i == 2) return _("Interval");
#else
        if (i == 0) return _("Mode: next");
        if (i == 1) return _("Interval: next");
#endif
        return _("Stop beacon");
#endif
      default: return "";
    }
  }

  void stepPowerProfile(int dir) {
    if (!_node_prefs) return;
    bool fem = _task->canControlFemLna();
    bool cpu_ok = _task->canSelectMcuSleep() && !_task->gpsBlocksCpuSleep() && !_task->bleBlocksCpuSleep();
    bool boost = _node_prefs->rx_boosted_gain != 0;
    bool lna = fem && _node_prefs->radio_fem_rxgain != 0;
    bool cpu = cpu_ok && _node_prefs->mcu_sleep != 0;
    PowerChoice choices[6];
    int n = buildPowerChoices(fem, cpu_ok, choices);
    if (n <= 0) return;
    int cur = -1;
    for (int i = 0; i < n; i++) {
      if (choices[i].boost == boost && choices[i].lna == lna && choices[i].cpu == cpu) { cur = i; break; }
    }
    int next = (cur < 0) ? 0 : ((cur + (dir >= 0 ? 1 : n - 1)) % n);
#ifdef HELTEC_LORA_V4
    _power_boost = choices[next].boost;
    _power_lna = choices[next].lna;
    _power_cpu = choices[next].cpu;
    _power_name = choices[next].name;
    _power_apply = false;
    _power_confirm = true;
#else
    _task->applyPowerProfile(choices[next].boost, choices[next].lna, choices[next].cpu);
#endif
  }

  void toggleRxBoost() {
    if (!_node_prefs) return;
    bool fem = _task->canControlFemLna();
    bool lna = fem && _node_prefs->radio_fem_rxgain != 0;
    bool cpu = _task->canSelectMcuSleep() && !_task->gpsBlocksCpuSleep() && !_task->bleBlocksCpuSleep()
               && _node_prefs->mcu_sleep != 0;
    _task->applyPowerProfile(_node_prefs->rx_boosted_gain == 0, lna, cpu);
  }

  void runAction(uint8_t i) {
    switch (_page) {
      case HomePage::RADIO:
        if (i == 0) {
          _radio_target = radioNextProfile(the_mesh.radioProfile());
          _radio_apply = false;
          _radio_confirm = true;
        }
        else if (i == 1) toggleRxBoost();
        else _task->toggleFemLna();
        break;
      case HomePage::POWER:
        stepPowerProfile(i == 0 ? 1 : -1);
        break;
      case HomePage::TIME:
        if (i == 0) requestClock();
        else enterSub(HomePage::CLOCK_NODES);
        break;
      case HomePage::CLOCK_NODES:
        if (i == 0) {
          _pick_idx = 0;
          enterSub(HomePage::NODE_PICKER);
        } else if (i == 1) {
          requestClock();
        } else if (usedClockNodes() == 0) {
          _task->showAlert(_("no nodes"), 1200);
        } else {
          clockNodesClear(_node_prefs->clock_nodes);
          the_mesh.savePrefs();
          _task->showAlert(_("nodes cleared"), 1200);
        }
        break;
      case HomePage::NODE_PICKER:
        addClockNodeFromPicker();
        break;
#if ENV_INCLUDE_GPS == 1
#ifdef HELTEC_LORA_V4
      case HomePage::GPS:
        _task->toggleGPS();
        break;
#endif
      case HomePage::BEACON:
#ifdef HELTEC_LORA_V4
        _beacon_sel = 0;
        if (i == 0) enterSub(HomePage::BEACON_MODE);
        else if (i == 1) openBeaconRecipient(BEACON_OFF);
        else if (i == 2) enterSub(HomePage::BEACON_INTERVAL);
        else enterSub(HomePage::BEACON_STOP);
#else
        if (i == 0) {
          the_mesh.cycleBeacon();
        } else if (i == 1) {
          the_mesh.cycleBeaconInterval();
        } else {
          the_mesh.stopBeacon();
          _task->showAlert(_("beacon stopped"), 900);
          break;
        }
        {
          char mode[20], target[32], pos[40], sent[24], alert[48];
          the_mesh.formatBeacon(mode, sizeof(mode), target, sizeof(target), pos, sizeof(pos), sent, sizeof(sent));
          snprintf(alert, sizeof(alert), "%s", mode);
          _task->notify(UIEventType::ack);
          _task->showAlert(alert, 900);
        }
#endif
        break;
#endif
      default: break;
    }
  }

  uint8_t setupRowCount() const { return _task->isEink() ? 4 : 3; }

  void stepSetupRow(int dir) {
    int n = setupRowCount();
    int next = (int)_setup_row + dir;
    if (next < 0) next = n - 1;
    if (next >= n) next = 0;
    _setup_row = (uint8_t)next;
  }

  void changeSetupValue() {
    if (!_node_prefs) return;
    bool eink = _task->isEink();
    const uint8_t lang_row = eink ? 2 : 1;
    if (_setup_row == 0) {
      _node_prefs->clock_saver = _node_prefs->clock_saver ? 0 : 1;
    } else if (eink && _setup_row == 1) {
      uint8_t mode = _node_prefs->eink_refresh;
      if (mode >= EINK_REFRESH_COUNT) mode = 0;
      _node_prefs->eink_refresh = (uint8_t)((mode + 1) % EINK_REFRESH_COUNT);
    } else if (_setup_row == lang_row) {
      _node_prefs->ui_lang = (_node_prefs->ui_lang == UI_LANG_EN) ? UI_LANG_RU : UI_LANG_EN;
    } else {
      _node_prefs->screen_off_idx = (uint8_t)((_node_prefs->screen_off_idx + 1) % 5);
    }
    the_mesh.savePrefs();
    _task->syncDisplayRefresh();
    _task->notify(UIEventType::ack);
  }

  uint8_t stepPage(int dir) {
#ifdef HELTEC_LORA_V4
    // V4 companion: information screens in a fixed, bidirectional cycle.
    static const uint8_t roots[] = {
      HomePage::FIRST, HomePage::MESSAGES, HomePage::BLUETOOTH,
      HomePage::NEIGHBORS, HomePage::RADIO,
#if ENV_INCLUDE_GPS == 1
      HomePage::GPS, HomePage::BEACON,
#endif
      HomePage::FSCAN, HomePage::RECENT, HomePage::SCAN,
#if UI_SENSORS_PAGE == 1
      HomePage::SENSORS,
#endif
      HomePage::MENU
    };
    const int count = sizeof(roots) / sizeof(roots[0]);
    for (int i = 0; i < count; i++) {
      if (roots[i] == _page) return roots[(i + (dir < 0 ? count - 1 : 1)) % count];
    }
    return roots[0];
#else
    uint8_t page = _page;
    for (uint8_t n = 0; n < HomePage::Count; n++) {
      int next = (int)page + dir;
      if (next < 0) next += HomePage::Count;
      else if (next >= HomePage::Count) next -= HomePage::Count;
      page = (uint8_t)next;
      if (isPageVisible(page)) return page;
    }
    return _page;
#endif
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0),
       _setup_row(0), _setup_edit(false), _shutdown_init(false), scan_peak(-127), fscan_state(FSCAN_IDLE), fscan_n(0),
       sensors_lpp(200) {  }

  bool allowsClock() const {
    if (_node_prefs && _node_prefs->clock_saver == 0) return false;
    if (_page == HomePage::SCAN || _page == HomePage::FSCAN) return false;
    if (_page == HomePage::NEIGHBORS) return false;
#if ENV_INCLUDE_GPS == 1
    if (_page == HomePage::BEACON) return false;
#ifdef HELTEC_LORA_V4
    if (isBeaconPicker()) return false;
#endif
#endif
    return true;
  }

#if UI_DISCOVER_SCREEN
  bool sendDiscoverRequest() {
    uint8_t cmd_bytes[6];
    cmd_bytes[0] = CTL_TYPE_NODE_DISCOVER_REQ | 1; // DISCOVER_REQ | prefix only
    cmd_bytes[1] = 0xFF;     // Repeaters
    the_mesh.getRNG()->random((uint8_t *) &disc_node_req_tag, 4);  // generate random tag
    memcpy(&cmd_bytes[2], &disc_node_req_tag, 4);
    disc_nodes_count = 0;
    mesh::Packet* req = the_mesh.createControlData(cmd_bytes, sizeof(cmd_bytes));
    if (req) {
      the_mesh.sendZeroHop(req);
      discovery_req_time = millis();
      return true;
    }
    return false;
  }

  void handleDiscoverResponse(const mesh::Packet* packet) {
    if (disc_nodes_count < DISCOVERED_NODES_TABLE_SIZE && memcmp(&packet->payload[2], &disc_node_req_tag, 4) == 0) {
      auto d = &discovered[disc_nodes_count++];
      memcpy(d->pubkey_prefix, &packet->payload[6], 8);
      d->type = packet->payload[0] & 0xF;
      d->snr_out = ((int8_t)packet->payload[1]) / 4.0;
      d->snr_in = radio_driver.getLastSNR();
      ContactInfo* c = the_mesh.lookupContactByPubKey(&packet->payload[6], 8);
      if (c != NULL) {
        strncpy(d->name, c->name, 32);
      } else {
        d->name[0] = 0;
      }
    }
  }
#endif

  void poll() override {
    if (_page == HomePage::SCAN || _page == HomePage::FSCAN) {
      _task->keepDisplayAwake();
    }
    if (_page == HomePage::SCAN && (long)(millis() - scan_next_sample) >= 0) {
      scan_next_sample = millis() + SCAN_SAMPLE_MILLIS;
      sampleNoise();
    }
    if (_page == HomePage::FSCAN && fscan_state == FSCAN_RUN) {
      stepFreqScan();
    }
    if (_shutdown_init && !_task->isButtonPressed()) {  // must wait for USR button to be released
      _task->shutdown();
    }
  }

  int render(DisplayDriver& display) override {
    setUiLang(_node_prefs ? _node_prefs->ui_lang : UI_LANG_EN);
    // Sub-screens are not part of the root cycle; only kick a page when we are
    // on a root screen that is not visible in this build.
    if (!isInSub() && !isPageVisible(_page)) _page = stepPage(1);

    char tmp[80];
    bool compact_header = display.width() == 128 && !display.isEink();
#ifdef HELTEC_LORA_V4
    if (compact_header) {
      const bool nested = isInSub() || _act_open || _radio_confirm || _scan_confirm || _power_confirm;
      char hm[8] = "";
      if (!nested) {
        const uint32_t now = _rtc->getCurrentTime();
        if (now < 1000000000UL) snprintf(hm, sizeof(hm), "--:--");
        else formatClockHm(now, _node_prefs ? _node_prefs->tz_offset_mins : 0, hm, sizeof(hm));
      }
      renderV4Header(display, *_task, pageTitle(), nested, hm);
    } else
#endif
    {
    display.setColor(UIColor::title_bkg);
    display.fillRect(0, 0, display.width(), 12);
    // node name
    display.setTextSize(1);
    display.setColor(UIColor::title_txt);
    char filtered_name[40];
    display.translateUTF8ToBlocks(filtered_name, compact_header ? pageTitle() : _node_prefs->node_name, sizeof(filtered_name));
    int title_x = 0;
#ifdef PIN_BUZZER
    if (compact_header && _task->isBuzzerQuiet()) title_x = 10;
#endif
    display.drawTextEllipsized(title_x, 2, compact_header ? 64 - title_x : display.width() - 40, filtered_name);

    // battery voltage
    renderBatteryIndicator(display, _task->getBattMilliVolts(), false, _task->isBuzzerQuiet(),
                           display.width() - 2, 1);

    // curr page indicator
    if (UIColor::title_bkg == UIColor::window_bkg) {
      display.setColor(UIColor::title_txt);
    } else {
      display.setColor(UIColor::title_bkg);
    }
    int y = 14;
    uint8_t visible_count = 0;
    uint8_t visible_index = 0;
    for (uint8_t i = 0; i < HomePage::Count; i++) {
      if (!isPageVisible(i)) continue;
      if (i == _page) visible_index = visible_count;
      visible_count++;
    }
    if (compact_header) {
      display.setColor(UIColor::title_txt);
      if (isInSub()) {
        display.drawTextRightAlign(97, 2, "<");
      } else {
        snprintf(tmp, sizeof(tmp), "%u/%u", visible_index + 1, visible_count);
        display.drawTextRightAlign(97, 2, tmp);
      }
      display.fillRect(0, 12, display.width(), 1);
    } else {
      int x = display.width() / 2 - 5 * (visible_count > 0 ? visible_count - 1 : 0);
      uint8_t drawn = 0;
      for (uint8_t i = 0; i < HomePage::Count; i++) {
        if (!isPageVisible(i)) continue;
        if (drawn == visible_index) display.fillRect(x-1, y-1, 4, 4);
        else display.fillRect(x, y, 2, 2);
        x += 10;
        drawn++;
      }
    }

    }

    // The OLED uses one content boundary, including names and translated labels
    // whose pixel widths vary. Keep the other displays' coordinates intact.
#ifdef HELTEC_LORA_V4
    const bool padded = compact_header && display.height() == 64;
    const int inset = padded ? OLED_INSET : 0;
#else
    const bool padded = false;
    const int inset = 0;
#endif
    const int row_right = display.width() - (padded ? inset : 1);
    const int column_gap = padded ? inset : 0;
    const int frame_inset = padded ? inset : 1;
    const int last_text_y = display.height() - inset - 9; // Cyrillic cell ends 8 px below cursor
    auto textY = [&](int y) { return padded && y > last_text_y ? last_text_y : y; };
    auto drawClipped = [&](int x, int y, int width, const char* text) {
      if (padded) {
        if (x < inset) x = inset;
        if (width > row_right - x) width = row_right - x;
        if (width <= 0) return;
        char line[96];
        display.translateUTF8ToBlocks(line, text, sizeof(line));
        if (width < display.getTextWidth("...") && display.getTextWidth(line) > width) return;
        display.drawTextEllipsized(x, textY(y), width, line);
      } else {
        display.drawTextEllipsized(x, y, width, text);
      }
    };
    auto drawLeft = [&](int x, int y, const char* text) {
      if (padded) drawClipped(x, y, row_right - x, text);
      else display.drawTextLeftAlign(x, y, text);
    };
    auto drawRight = [&](int x, int y, const char* text) {
      if (padded && x > row_right) x = row_right;
      if (padded && display.getTextWidth(text) > x - inset) drawClipped(inset, y, x - inset, text);
      else display.drawTextRightAlign(x, textY(y), text);
    };
    auto drawCentered = [&](int x, int y, const char* text) {
      if (padded) {
        int width = display.getTextWidth(text);
        if (x - width / 2 < inset || x + (width + 1) / 2 > row_right) {
          drawClipped(inset, y, row_right - inset, text);
          return;
        }
      }
      display.drawTextCentered(x, textY(y), text);
    };

    if (_page == HomePage::FIRST) {
#ifdef HELTEC_LORA_V4
      if (compact_header) {
        // V4 home: connection state, the queue for the phone, the local store and
        // the date. No big MSG counter: the numbers read better on Messages.
        display.setTextSize(1);
        char buf[48];
        int y = 17;
  #ifdef ENABLE_WIFI_INTERFACE
        if (_task->isWiFiEnabled() && WiFi.status() == WL_CONNECTED) {
          IPAddress ip = WiFi.localIP();
          display.setColor(UIColor::primary_txt);
          snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        } else
  #endif
        if (_task->hasConnection()) {
          display.setColor(UIColor::primary_txt);
          snprintf(buf, sizeof(buf), "%s", _("client connected"));
        } else if (_task->isBluetoothEnabled() && the_mesh.getBLEPin() != 0) {
          display.setColor(UIColor::warning_txt);
          snprintf(buf, sizeof(buf), "PIN %06lu", (unsigned long)the_mesh.getBLEPin());
        } else {
          display.setColor(UIColor::secondary_txt);
          snprintf(buf, sizeof(buf), "%s", _("no client"));
        }
        drawLeft(0, y, buf);
        y += 12;

        display.setColor(UIColor::primary_txt);
        snprintf(buf, sizeof(buf), _("Phone queue %d"), _task->getMsgCount());
        drawLeft(0, y, buf);
        y += 12;
        snprintf(buf, sizeof(buf), _("Stored %d"), _task->getDeviceStored());
        drawLeft(0, y, buf);
        y += 12;

        display.setColor(UIColor::secondary_txt);
        if (!display.isEink() && _rtc->getCurrentTime() >= 1000000000UL) {
          int16_t tz = _node_prefs ? _node_prefs->tz_offset_mins : 0;
          uint32_t now = _rtc->getCurrentTime();
          char date_buf[24];
          formatClockDate(now, tz, _node_prefs ? _node_prefs->ui_lang : UI_LANG_RU,
                          date_buf, sizeof(date_buf));
          snprintf(buf, sizeof(buf), "%s", date_buf);
        } else {
          snprintf(buf, sizeof(buf), "%s", _("time not set"));
        }
        drawLeft(0, y, buf);
      } else
#endif
      {
        display.setColor(UIColor::primary_txt);
        // MSG: still waiting for the phone / still kept on this device
        display.setTextSize(2);
        sprintf(tmp, "MSG %d/%d", _task->getMsgCount(), _task->getDeviceStored());
        drawCentered(display.width() / 2, 20, tmp);

        bool show_date = !display.isEink() && _rtc->getCurrentTime() >= 1000000000UL;
        // hasConnection() reports any transport, not necessarily Wi-Fi. Show the
        // Wi-Fi address only when that link is actually up; otherwise the generic
        // indicator (USB / BLE client), and the PIN when nothing is attached.
        if (_task->hasConnection()) {
          display.setColor(UIColor::warning_txt);
          #ifdef WIFI_SSID
          if (WiFi.status() == WL_CONNECTED) {
            IPAddress ip = WiFi.localIP();
            snprintf(tmp, sizeof(tmp), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
          } else {
            snprintf(tmp, sizeof(tmp), "< Connected >");
          }
          #else
          snprintf(tmp, sizeof(tmp), "< Connected >");
          #endif
          display.setTextSize(1);
          #ifdef UI_SHOW_CLOCK
          drawCentered(display.width() / 2, 110, tmp);
          #else
          drawCentered(display.width() / 2, 43, tmp);
          #endif
        } else if (the_mesh.getBLEPin() != 0) { // BT pin
          display.setColor(UIColor::warning_txt);
          bool room = !show_date || display.height() > 64;
          display.setTextSize(room ? 2 : 1);
          sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
          #ifdef UI_SHOW_CLOCK
          display.setTextSize(1);
          drawCentered(display.width() / 2, 110, tmp);
          #else
          // A size-2 PIN at y=43 overlaps the date at y=53 on the 64px OLED.
          display.setTextSize(show_date && display.height() <= 64 ? 1 : 2);
          drawCentered(display.width() / 2, 43, tmp);
          #endif
        }
        if (show_date) {
          int16_t tz = _node_prefs ? _node_prefs->tz_offset_mins : 0;
          uint32_t now = _rtc->getCurrentTime();
          char hm[8];
          char date_buf[24];
          char line[36];
          formatClockHm(now, tz, hm, sizeof(hm));
          formatClockDate(now, tz, _node_prefs ? _node_prefs->ui_lang : UI_LANG_RU,
                          date_buf, sizeof(date_buf));
          snprintf(line, sizeof(line), "%s  %s", hm, date_buf);
          display.setTextSize(1);
          display.setColor(UIColor::secondary_txt);
          drawCentered(display.width() / 2, display.height() - 11, line);
        }
      }
    } else if (_page == HomePage::MESSAGES) {
#ifdef HELTEC_LORA_V4
      if (compact_header) {
        // V4: counters only, the list and the text are separate screens.
        display.setTextSize(1);
        char buf[40];
        int y = 17;
        int unread = _task->getUnreadCount();
        display.setColor(UIColor::primary_txt);
        snprintf(buf, sizeof(buf), _("In memory: %d"), _task->getDeviceStored());
        drawLeft(0, y, buf); y += 12;
        snprintf(buf, sizeof(buf), _("To phone: %d"), _task->getMsgCount());
        drawLeft(0, y, buf); y += 12;
        display.setColor(unread > 0 ? UIColor::warning_txt : UIColor::secondary_txt);
        snprintf(buf, sizeof(buf), _("Unread %d"), unread);
        drawLeft(0, y, buf); y += 12;
        if (_task->getDeviceStored() == 0 && _task->getMsgCount() == 0) {
          display.setColor(UIColor::secondary_txt);
          drawLeft(0, y, _("no messages"));
        } else {
          display.setColor(UIColor::secondary_txt);
          drawLeft(0, y, _("hold: read"));
        }
      } else
#endif
      {
        // Root 2: message counters only. The preview itself is a separate screen
        // (long press here or on the main screen).
        display.setTextSize(1);
        int y = 20;
        char mbuf[32];
        display.setColor(UIColor::primary_txt);
        snprintf(mbuf, sizeof(mbuf), _("In memory: %d"), _task->getDeviceStored());
        drawLeft(0, y, mbuf);
        y += 12;
        snprintf(mbuf, sizeof(mbuf), _("To phone: %d"), _task->getMsgCount());
        drawLeft(0, y, mbuf);
        y += 12;
        bool any = _task->getDeviceStored() > 0 || _task->getMsgCount() > 0;
        display.setColor(any ? UIColor::primary_txt : UIColor::secondary_txt);
        drawLeft(0, y, any ? _("hold: read") : _("no messages"));
      }
    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      int shown = 0;
      display.setColor(UIColor::primary_txt);
      const int first_row_y = padded ? 18 : 20;
      int y = first_row_y;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += 11) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;  // empty slot
        shown++;
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }

        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = row_right - inset - timestamp_width - column_gap;

        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        drawClipped(0, y, max_name_width, filtered_recent_name);
        drawRight(row_right, y, tmp);
      }
      if (shown == 0) {
        display.setColor(UIColor::secondary_txt);
        drawCentered(display.width() / 2, 28, _("no adverts"));
      }
      // last slot stays free until the list is full, so real data fits there
      if (recent[UI_RECENT_LIST_SIZE - 1].name[0] == 0) {
        display.setColor(UIColor::secondary_txt);
        snprintf(tmp, sizeof(tmp), _("In list: %d"), shown);
        drawCentered(display.width() / 2, first_row_y + (UI_RECENT_LIST_SIZE - 1) * 11, tmp);
      }
    } else if (_page == HomePage::NEIGHBORS) {
      int n = the_mesh.getNeighbors(neighbor_list, NEIGHBOR_TABLE_SIZE);
      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);
      if (n == 0) {
        neighbors_scroll_offset = 0;
        neighbors_next_scroll = 0;
        display.setColor(UIColor::secondary_txt);
        drawCentered(display.width() / 2, 28, _("no neighbors"));
      } else {
        const int line_h = 11;
        int y0 = 20;
#ifdef HELTEC_LORA_V4
        if (compact_header) {
          display.setColor(UIColor::secondary_txt);
          snprintf(tmp, sizeof(tmp), _("Neighbors %d"), n);
          drawLeft(0, 18, tmp);
          display.setColor(UIColor::primary_txt);
          y0 = 29;
        }
#endif
        int visible = (display.height() - y0) / line_h;
        if (visible < 1) visible = 1;
        if (n <= visible) {
          neighbors_scroll_offset = 0;
          neighbors_next_scroll = 0;
        } else {
          if (neighbors_scroll_offset >= n) neighbors_scroll_offset = 0;
          if (neighbors_next_scroll == 0) {
            neighbors_next_scroll = millis() + NEIGHBOR_SCROLL_MS;
          } else if ((long)(millis() - neighbors_next_scroll) >= 0) {
            neighbors_scroll_offset = (neighbors_scroll_offset + 1) % n;
            neighbors_next_scroll = millis() + NEIGHBOR_SCROLL_MS;
          }
        }

        int rows = n < visible ? n : visible;
        for (int i = 0; i < rows; i++) {
          auto a = &neighbor_list[(neighbors_scroll_offset + i) % n];
          int y = y0 + i * line_h;
          float snr = a->snr_x4 / 4.0f;
          sprintf(tmp, "%+.0f", snr);

          int snr_width = display.getTextWidth(tmp);
          int max_name_width = row_right - inset - snr_width - column_gap;

          char filtered_name[sizeof(a->name)];
          display.translateUTF8ToBlocks(filtered_name, a->name, sizeof(filtered_name));
          drawClipped(0, y, max_name_width, filtered_name);
          drawRight(row_right, y, tmp);
        }
      }
    } else if (_page == HomePage::RADIO) {
      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);
      if (_radio_confirm) {
        float freq = _node_prefs->freq, bw = _node_prefs->bw;
        float home_freq = _node_prefs->home_freq, home_bw = _node_prefs->home_bw;
        uint8_t sf = _node_prefs->sf, cr = _node_prefs->cr;
        uint8_t home_sf = _node_prefs->home_sf, home_cr = _node_prefs->home_cr;
        bool repeat = applyRadioProfileChoice(_radio_target, freq, bw, sf, cr,
                                              home_freq, home_bw, home_sf, home_cr);
        repeat = repeat && the_mesh.isRepeatFreqAllowed((uint32_t)(freq * 1000.0f + 0.5f));
        snprintf(tmp, sizeof(tmp), "%s > %s", radioProfileLabel(the_mesh.radioProfile()),
                 radioProfileLabel(_radio_target));
        drawClipped(0, 17, display.width(), tmp);
        snprintf(tmp, sizeof(tmp), "%.3f BW%g", freq, bw);
        drawClipped(0, 29, display.width(), tmp);
        snprintf(tmp, sizeof(tmp), "SF%u CR%u  %s", (unsigned)sf, (unsigned)cr,
                 repeat ? _("RPT on") : _("RPT off"));
        drawClipped(0, 41, display.width(), tmp);
        display.setColor(_radio_apply ? UIColor::primary_txt : UIColor::warning_txt);
        drawLeft(0, 53, _radio_apply ? _("Cancel") : _("> Cancel"));
        display.setColor(_radio_apply ? UIColor::warning_txt : UIColor::primary_txt);
        drawRight(display.width() - 1, 53, _radio_apply ? _("> Apply") : _("Apply"));
      } else if (compact_header) {
        if (!_radio_details) {
          // The receiver already maintains this estimate; do not tune away or
          // start a scan merely to populate the tiny history graph.
          if (fabsf(_node_prefs->freq - _nf_freq) > 0.0005f ||
              fabsf(_node_prefs->bw - _nf_bw) > 0.001f ||
              _node_prefs->sf != _nf_sf || _node_prefs->cr != _nf_cr) {
            _nf_freq = _node_prefs->freq; _nf_bw = _node_prefs->bw;
            _nf_sf = _node_prefs->sf; _nf_cr = _node_prefs->cr;
            _nf_count = _nf_next = 0;
            _nf_valid_at = millis() + 2000;
          }
          int nf = (long)(millis() - _nf_valid_at) >= 0 ? radio_driver.getNoiseFloor() : 0;
          if (nf != 0 && (_nf_count == 0 || (long)(millis() - _nf_sample_at) >= 0)) {
            _nf_history[_nf_next] = (int16_t)nf;
            _nf_next = (_nf_next + 1) % 12;
            if (_nf_count < 12) _nf_count++;
            _nf_sample_at = millis() + 2000;
          }
          display.setTextSize(2);
          snprintf(tmp, sizeof(tmp), "%.3f", _node_prefs->freq);
          drawCentered(54, 17, tmp);
          display.setTextSize(1);
          drawRight(display.width() - 1, 25, "MHz");
          snprintf(tmp, sizeof(tmp), "BW%g SF%u CR%u TX%d", _node_prefs->bw,
                   (unsigned)_node_prefs->sf, (unsigned)_node_prefs->cr,
                   (int)_node_prefs->tx_power_dbm);
          const int frame_width = display.width() - 2 * frame_inset;
          if (padded && display.getTextWidth(tmp) > frame_width - 2) {
            snprintf(tmp, sizeof(tmp), "BW%g SF%uCR%uTX%d", _node_prefs->bw,
                     (unsigned)_node_prefs->sf, (unsigned)_node_prefs->cr,
                     (int)_node_prefs->tx_power_dbm);
          }
          display.drawRect(frame_inset, 36, frame_width, 12);
          drawCentered(display.width() / 2, 39, tmp);
          display.setColor(UIColor::secondary_txt);
          if (nf != 0) snprintf(tmp, sizeof(tmp), "NF %d dBm", nf);
          else snprintf(tmp, sizeof(tmp), "NF --");
          drawLeft(0, 53, tmp);
          const int graph_w = 50;
          const int graph_x = padded ? row_right - graph_w : 78;
          const int graph_bottom = padded ? 59 : 61;
          display.drawRect(graph_x, graph_bottom - 11, graph_w, 13);
          for (uint8_t i = 0; i < _nf_count; i++) {
            int v = _nf_history[(_nf_next + 12 - _nf_count + i) % 12];
            int h = constrain((v + 125) * 10 / 55, 1, 10);
            display.fillRect(graph_x + 2 + i * 4, graph_bottom - h, 3, h);
          }
        } else {
          drawLeft(0, 18, radioProfileLabel(the_mesh.radioProfile()));
          drawRight(127, 18, _node_prefs->isRepeatEn() ? _("Repeat: on") : _("Repeat: off"));
          drawLeft(0, 30, _node_prefs->rx_boosted_gain ? _("RXB: on") : _("RXB: off"));
          if (_task->canControlFemLna()) {
            drawRight(127, 30, _task->isFemLnaEnabled() ? _("LNA: on") : _("LNA: off"));
          }
          snprintf(tmp, sizeof(tmp), "%.3f MHz", _node_prefs->freq);
          drawLeft(0, 42, tmp);
          snprintf(tmp, sizeof(tmp), "BW%g SF%u CR%u TX%d", _node_prefs->bw,
                   (unsigned)_node_prefs->sf, (unsigned)_node_prefs->cr,
                   (int)_node_prefs->tx_power_dbm);
          drawLeft(0, 54, tmp);
        }
      } else {
      // freq / sf
      display.setCursor(0, 20);
      sprintf(tmp, "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);

      display.setCursor(0, 31);
      sprintf(tmp, "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);

      // tx power,  noise floor
      display.setCursor(0, 42);
      if (_task->canControlFemLna()) {
        sprintf(tmp, "TX: %ddBm  LNA: %s", _node_prefs->tx_power_dbm,
                _task->isFemLnaEnabled() ? _("on") : _("off"));
      } else {
        // no controllable FEM LNA on this board - don't advertise a dead control
        sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      }
      display.print(tmp);
      display.setCursor(0, 53);
      drawLeft(0, 53, radioProfileLabel(the_mesh.radioProfile()));
      // RX Boost is an SX1262 gain setting, unrelated to the FEM LNA: keep it
      // on its own row instead of hiding it behind a gesture hint.
      display.setColor(UIColor::secondary_txt);
      drawRight(display.width() - 1, 53,
        (_node_prefs && _node_prefs->rx_boosted_gain) ? _("RXB: on") : _("RXB: off"));
      }
    } else if (_page == HomePage::POWER) {
#ifdef HELTEC_LORA_V4
      if (_power_confirm) {
        display.setTextSize(1);
        display.setColor(UIColor::primary_txt);
        drawLeft(0, 18, _power_name);
        drawRight(display.width() - 1, 18, _power_cpu ? _("Sleep: yes") : _("Sleep: no"));
        drawLeft(0, 30, _power_boost ? _("RXB: on") : _("RXB: off"));
        if (_task->canControlFemLna())
          drawRight(display.width() - 1, 30, _power_lna ? _("LNA: on") : _("LNA: off"));
        display.setColor(_power_apply ? UIColor::primary_txt : UIColor::warning_txt);
        drawLeft(0, 42, _power_apply ? _("Cancel") : _("> Cancel"));
        display.setColor(_power_apply ? UIColor::warning_txt : UIColor::primary_txt);
        drawLeft(0, 54, _power_apply ? _("> Apply") : _("Apply"));
      } else {
#endif
      bool fem = _task->canControlFemLna();
      bool cpu_hw = _task->canSelectMcuSleep();
      bool cpu_ok = cpu_hw && !_task->gpsBlocksCpuSleep() && !_task->bleBlocksCpuSleep();
      bool boost = _node_prefs->rx_boosted_gain != 0;
      bool lna = fem && _node_prefs->radio_fem_rxgain != 0;
      bool cpu = cpu_ok && _node_prefs->mcu_sleep != 0;
      PowerChoice choices[6];
      int n = buildPowerChoices(fem, cpu_ok, choices);
      const char* name = "Custom";
      for (int i = 0; i < n; i++) {
        if (choices[i].boost == boost && choices[i].lna == lna && choices[i].cpu == cpu) {
          name = choices[i].name;
          break;
        }
      }

      // Beacon uses these rows on the 128x64 OLED: title is 12px, dots at y=14.
      display.setTextSize(1);
      int y = 18;
      display.setColor(UIColor::primary_txt);
      drawLeft(0, y, _("Mode"));
      display.setColor(UIColor::secondary_txt);
      drawRight(display.width() - 1, y, name);
      y += 12;
      // "Sleep allowed" is the honest wording: the CPU still runs while the UI
      // is being drawn, the profile only permits light sleep between frames.
      display.setColor(UIColor::primary_txt);
      drawLeft(0, y, !cpu_hw ? _("Sleep: n/a") : (cpu ? _("Sleep: yes") : _("Sleep: no")));
      drawRight(display.width() - 1, y, boost ? _("RX: on") : _("RX: off"));
      y += 12;
      if (fem) {
        drawLeft(0, y, lna ? _("LNA: on") : _("LNA: off"));
        y += 12;
      }
      if (y + 8 <= display.height()) {
        // A real reason instead of a gesture hint.
        const char* hold = NULL;
        if (!cpu_hw) hold = _("sleep n/a");
        else if (_task->gpsBlocksCpuSleep()) hold = _("GPS holds CPU");
        else if (_task->bleBlocksCpuSleep()) hold = _("BLE holds CPU");
        if (hold) {
          display.setColor(UIColor::warning_txt);
          drawCentered(display.width() / 2, y, hold);
        }
      }
#ifdef HELTEC_LORA_V4
      }
#endif
    } else if (_page == HomePage::SCAN) {
      if (scan_cnt == 0) sampleNoise();
      if (scan_cnt > 0) {
        scan_live = (int16_t)((scan_sum - (int32_t)scan_cnt / 2) / (int32_t)scan_cnt);
      }
      int live = scan_live;
      bool pkt = scan_pkt;
      scan_sum = 0;
      scan_cnt = 0;
      scan_pkt = false;
      int nl = radio_driver.getNoiseFloor();

      display.setColor(UIColor::primary_txt);
      display.setTextSize(2);
      sprintf(tmp, "%d", live);
      drawCentered(display.width() / 2, 18, tmp);

      display.setTextSize(1);
      if (pkt) {
        display.setColor(UIColor::warning_txt);
        drawRight(display.width() - 1, 18, "PKT");
      }

      display.setColor(UIColor::secondary_txt);
      if (nl == 0) {
        sprintf(tmp, "NL --");
      } else {
        sprintf(tmp, "NL %d", nl);
      }
      drawLeft(0, 36, tmp);
      if (scan_peak < -120) {
        sprintf(tmp, "pk --");
      } else {
        sprintf(tmp, "pk %d", (int)scan_peak);
      }
      drawRight(display.width() - 1, 36, tmp);

      const int rssi_min = -120;
      const int rssi_max = -50;
      int bar_x = padded ? inset : 2;
      int bar_y = 48;
      int bar_h = 8;
      int bar_w = display.width() - 2 * bar_x;
      int clamped = live;
      if (clamped < rssi_min) clamped = rssi_min;
      if (clamped > rssi_max) clamped = rssi_max;
      int fill = ((clamped - rssi_min) * bar_w) / (rssi_max - rssi_min);

      display.setColor(UIColor::secondary_txt);
      display.drawRect(bar_x, bar_y, bar_w, bar_h);
      display.setColor(UIColor::primary_txt);
      if (fill > 2) {
        display.fillRect(bar_x + 1, bar_y + 1, fill - 2, bar_h - 2);
      }
    } else if (_page == HomePage::FSCAN) {
      display.setTextSize(1);
#ifdef HELTEC_LORA_V4
      if (_scan_confirm) {
        display.setColor(UIColor::primary_txt);
        drawLeft(0, 18, _("RX pauses for scan"));
        display.setColor(_scan_apply ? UIColor::primary_txt : UIColor::warning_txt);
        drawLeft(0, 34, _scan_apply ? _("Cancel") : _("> Cancel"));
        display.setColor(_scan_apply ? UIColor::warning_txt : UIColor::primary_txt);
        drawLeft(0, 46, _scan_apply ? _("> Start scan") : _("Start scan"));
      } else
#endif
      if (fscan_state == FSCAN_RUN) {
        display.setColor(UIColor::primary_txt);
        if (fscan_n > 0) {
          sprintf(tmp, "scan %d/%d", (int)fscan_i, (int)fscan_n);
          drawCentered(display.width() / 2, 22, tmp);
          if (fscan_i < fscan_n) {
            sprintf(tmp, "%06.3f", fscan[fscan_i].freq);
            drawCentered(display.width() / 2, 36, tmp);
          }
          int bar_x = padded ? inset : 2;
          int bar_w = display.width() - 2 * bar_x;
          int fill = ((int)fscan_i * bar_w) / fscan_n;
          display.setColor(UIColor::secondary_txt);
          display.drawRect(2, 50, bar_w, 8);
          display.setColor(UIColor::primary_txt);
          if (fill > 2) display.fillRect(3, 51, fill - 2, 6);
        }
      } else if (fscan_state == FSCAN_DONE) {
        display.setColor(UIColor::secondary_txt);
        sprintf(tmp, "now %06.3f", _node_prefs->freq);
        drawLeft(0, 18, tmp);
        display.setColor(UIColor::primary_txt);
        int y = 29;
        for (int i = 0; i < 3; i++, y += 11) {
          if (i >= fscan_n) break;
          bool here = fabsf(fscan_best[i].freq - _node_prefs->freq) < 0.001f;
          sprintf(tmp, "%06.3f  %d%s", fscan_best[i].freq, (int)fscan_best[i].rssi, here ? " *" : "");
          drawLeft(0, y, tmp);
        }
      } else {
        display.setColor(UIColor::secondary_txt);
        drawCentered(display.width() / 2, 28, _("find quiet freq"));
      }
    } else if (_page == HomePage::BLUETOOTH) {
      // Adapter state and link state are different things: the middle column is
      // the radio being enabled, the right column says whether a peer is attached.
      // USB has no client detector in this firmware, so it is reported as unknown.
      display.setTextSize(1);
      char buf[40];
      int y = 18;
      const int line_h = 12;

      bool ble_on = _task->isBluetoothEnabled();
      bool wifi_on = _task->isWiFiEnabled();
      bool usb_on = _task->isUSBEnabled();
      bool ble_link = _task->isBluetoothConnected();
      bool wifi_link = _task->isWiFiConnected() || _task->isWiFi2Connected();

      // Fixed columns: Cyrillic is multi-byte in UTF-8, so printf field widths
      // would misalign the rows. Draw each column at an explicit x instead.
      display.setColor(UIColor::primary_txt);
      drawLeft(0, y, "BLE");
      drawLeft(36, y, ble_on ? _("on") : _("off"));
      if (ble_on) drawLeft(72, y, ble_link ? _("client") : _("no"));
      y += line_h;

      drawLeft(0, y, "WiFi");
      drawLeft(36, y, wifi_on ? _("on") : _("off"));
      if (wifi_on) drawLeft(72, y, wifi_link ? _("client") : _("no"));
      y += line_h;

      // USB: the serial transport always reports _("connected"), so show "?"
      drawLeft(0, y, "USB");
      drawLeft(36, y, usb_on ? _("on") : _("off"));
      if (usb_on) drawLeft(72, y, "?");
      y += line_h;

#ifdef HELTEC_LORA_V4
      if (compact_header) {
        display.setColor(UIColor::secondary_txt);
#ifdef ENABLE_WIFI_INTERFACE
        if (wifi_on && WiFi.status() == WL_CONNECTED) {
          IPAddress ip = WiFi.localIP();
          snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
          drawLeft(0, y, buf);
        } else
#endif
        if (!ble_link && !wifi_link && ble_on && the_mesh.getBLEPin() != 0) {
          snprintf(buf, sizeof(buf), "PIN %06lu", (unsigned long)the_mesh.getBLEPin());
          drawLeft(0, y, buf);
        }
      } else
#endif
      {
        display.setColor(UIColor::secondary_txt);
        if (the_mesh.getBLEPin() != 0) {
          // While a client holds the link the PIN is not needed, so the row shows
          // the Wi-Fi network name instead of repeating the pairing code.
          if (ble_link || wifi_link) {
#ifdef ENABLE_WIFI_INTERFACE
            snprintf(buf, sizeof(buf), "%.20s", wifi_on ? WiFi.SSID().c_str() : _("connected"));
#else
            snprintf(buf, sizeof(buf), "%s", _("connected"));
#endif
          } else {
            snprintf(buf, sizeof(buf), "Pin:%06lu", (unsigned long)the_mesh.getBLEPin());
          }
          drawLeft(0, y, buf);
        }
      }
    } else if (_page == HomePage::CONNECTIONS) {
#ifdef HELTEC_LORA_V4
      if (_conn_confirm) {
        const char* names[] = {"BLE", "Wi-Fi", "USB"};
        snprintf(tmp, sizeof(tmp), _conn_was_enabled ? _("Disable %s?") : _("Enable %s?"), names[_conn_selected]);
        display.setTextSize(1);
        display.setColor(UIColor::primary_txt);
        drawLeft(0, 18, tmp);
        display.setColor(_conn_apply ? UIColor::primary_txt : UIColor::warning_txt);
        drawLeft(0, 34, _conn_apply ? _("Cancel") : _("> Cancel"));
        display.setColor(_conn_apply ? UIColor::warning_txt : UIColor::primary_txt);
        drawLeft(0, 46, _conn_apply ? _("> Apply") : _("Apply"));
      } else {
#endif
      // CONNECTIONS sub-screen (long press on BLUETOOTH):
      // click = move the marker, hold = toggle the selected interface,
      // double click = back to BLUETOOTH
      int y = 18;
      const int line_h = 12;
      int selected = _conn_selected;
      int conn_count = 0;
      display.setTextSize(1);
      char buf[40];

      const char* names[3] = { "BLE", "WiFi", "USB" };
      bool states[3] = { _task->isBluetoothEnabled(), _task->isWiFiEnabled(),
                         _task->isUSBEnabled() };
      for (int i = 0; i < 3; i++) {
        display.setColor(selected == i ? UIColor::warning_txt : UIColor::primary_txt);
        drawLeft(0, y, selected == i ? ">" : " ");
        drawLeft(10, y, names[i]);
        drawLeft(46, y, states[i] ? _("on") : _("off"));
        y += line_h;
        conn_count++;
      }
      (void)conn_count;

      // Data row: the two TCP servers are separate links, so report them apart.
      // 5000 and 5001 are separate client ports; the peer identity is unknown.
      display.setColor(UIColor::secondary_txt);
      if (_task->isWiFiEnabled()) {
        snprintf(buf, sizeof(buf), "5000:%s 5001:%s",
                 _task->isWiFiConnected() ? "+" : "-",
                 _task->hasWiFi2() ? (_task->isWiFi2Connected() ? "+" : "-") : _("n/a"));
      } else {
        snprintf(buf, sizeof(buf), _("ports: --"));
      }
      drawLeft(0, y, buf);
#ifdef HELTEC_LORA_V4
      }
#endif

#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
#ifdef HELTEC_LORA_V4
      if (compact_header) {
        display.setTextSize(1);
        display.setColor(UIColor::primary_txt);
        if (!nmea) {
          drawLeft(0, 17, _("GPS unavailable"));
          drawLeft(0, 29, _("check the wires"));
        } else if (!_task->getGPSState()) {
          drawLeft(0, 17, _("module off"));
          drawLeft(0, 29, nmea->isEnabled() ? _("still powered") : _("powered off"));
        } else {
          const bool fix = nmea->isValid();
          const int sats = nmea->satellitesCount();
          snprintf(buf, sizeof(buf), _("SAT %d"), sats);
          drawClipped(0, 18, 58, buf);
          const int bar_x = 62;
          const int bar_w = row_right - bar_x;
          display.drawRect(bar_x, 17, bar_w, 10);
          const int fill = constrain(sats, 0, 12) * (bar_w - 4) / 12;
          if (fill > 0) display.fillRect(bar_x + 2, 19, fill, 6);
          if (fix) snprintf(buf, sizeof(buf), _("ALT %.0f m"), nmea->getAltitude() / 1000.0);
          else snprintf(buf, sizeof(buf), "%s", _("ALT --"));
          const char* speed = fix ? "--.-km/h" : _("searching");
          drawClipped(0, 30, row_right - inset - display.getTextWidth(speed) - column_gap, buf);
          drawRight(row_right, 30, speed);
          if (fix) snprintf(buf, sizeof(buf), _("LAT %.6f"), nmea->getLatitude() / 1000000.0);
          else snprintf(buf, sizeof(buf), "%s", _("LAT --"));
          drawLeft(0, 40, buf);
          if (fix) snprintf(buf, sizeof(buf), _("LON %.6f"), nmea->getLongitude() / 1000000.0);
          else snprintf(buf, sizeof(buf), "%s", _("LON --"));
          drawLeft(0, 52, buf);
        }
      } else {
#endif
      int y = 20;
      bool gps_state = _task->getGPSState();
      const char* module = gps_state ? _("on") : _("off");
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        module = gps_state ? _("off(hw)") : _("off(sw)");
      }
#endif
      display.setColor(UIColor::secondary_txt);
      drawLeft(0, y, _("module"));
      display.setColor(gps_state ? UIColor::primary_txt : UIColor::warning_txt);
      drawRight(display.width() - 1, y, module);
      y += 12;
      if (nmea == NULL) {
        display.setColor(UIColor::secondary_txt);
        drawLeft(0, y, _("no GPS access"));
      } else if (!gps_state) {
        // isEnabled() reads the enable pin. With no pin it stays true, so the
        // chip can keep drawing current after the software switch is off.
        display.setColor(UIColor::secondary_txt);
        drawLeft(0, y, nmea->isEnabled() ? _("still powered") : _("powered off"));
        y += 12;
      } else {
        display.setColor(UIColor::secondary_txt);
        sprintf(buf, _("fix: %s"), nmea->isValid() ? _("yes") : _("no"));
        drawLeft(0, y, buf);
        display.setColor(UIColor::primary_txt);
        sprintf(buf, _("sats %d"), nmea->satellitesCount());
        drawRight(display.width() - 1, y, buf);
        y += 12;
        if (nmea->isValid()) {
          display.setColor(UIColor::secondary_txt);
          drawLeft(0, y, _("pos"));
          display.setColor(UIColor::primary_txt);
          sprintf(buf, "%.4f %.4f",
            nmea->getLatitude() / 1000000., nmea->getLongitude() / 1000000.);
          drawRight(display.width() - 1, y, buf);
          y += 12;
          if (y + 8 <= display.height()) {
            display.setColor(UIColor::secondary_txt);
            drawLeft(0, y, _("alt"));
            display.setColor(UIColor::primary_txt);
            sprintf(buf, "%.2f", nmea->getAltitude() / 1000.);
            drawRight(display.width() - 1, y, buf);
          }
        }
      }
#ifdef HELTEC_LORA_V4
      }
#endif
#ifdef HELTEC_LORA_V4
    } else if (isBeaconPicker()) {
      renderBeaconPicker(display);
#endif
    } else if (_page == HomePage::BEACON) {
      char mode[32];
      char target[32];
      char pos[40];
      char sent[24];
      the_mesh.formatBeacon(mode, sizeof(mode), target, sizeof(target), pos, sizeof(pos), sent, sizeof(sent));
      char kind[24];
      char iv[12];
      kind[0] = iv[0] = 0;
      const char* sp = strchr(mode, ' ');
      if (sp) {
        size_t klen = (size_t)(sp - mode);
        if (klen >= sizeof(kind)) klen = sizeof(kind) - 1;
        memcpy(kind, mode, klen);
        kind[klen] = 0;
        strncpy(iv, sp + 1, sizeof(iv) - 1);
        iv[sizeof(iv) - 1] = 0;
      } else {
        strncpy(kind, mode, sizeof(kind) - 1);
        kind[sizeof(kind) - 1] = 0;
      }
      int y = 18;
      display.setColor(UIColor::primary_txt);
      drawLeft(0, y, kind);
      if (iv[0]) {
        display.setColor(UIColor::secondary_txt);
        drawRight(display.width() - 1, y, iv);
      }
      y += 12;
      char shown[32];
      display.translateUTF8ToBlocks(shown, target, sizeof(shown));
      display.setColor(UIColor::primary_txt);
      drawClipped(0, y, display.width() - 1, shown);
      y += 12;
      display.setColor(strcmp(pos, _("no fix")) == 0 || strcmp(pos, _("no gps")) == 0
          ? UIColor::warning_txt : UIColor::secondary_txt);
      drawClipped(0, y, display.width() - 1, pos);
      y += 12;
      display.setColor(UIColor::secondary_txt);
      drawLeft(0, y, sent);
#ifndef HELTEC_LORA_V4
      if (display.height() > 80 && strcmp(kind, _("off")) != 0) {
        drawCentered(display.width() / 2, display.height() - 11, "3x time    4x off");
      }
#endif
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      int y = 18;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll?UI_SENSORS_ROWS:sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) { // reached end, reset
          r.reset();
          r.readHeader(channel, type);
        }

        float v;
        switch (type) {
          case LPP_GPS: // GPS
            float lat, lon, alt;
            r.readGPS(lat, lon, alt);
            strcpy(name, "GPS"); sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          case LPP_VOLTAGE:
            r.readVoltage(v);
            strcpy(name, _("vbat")); sprintf(buf, _("%5.2f V"), v);
            break;
          case LPP_CURRENT:
            r.readCurrent(v);
            strcpy(name, _("current")); sprintf(buf, _("%.2f A"), v);
            break;
          case LPP_TEMPERATURE:
            r.readTemperature(v);
            strcpy(name, _("temp")); sprintf(buf, "%.1f C", v);
            break;
          case LPP_RELATIVE_HUMIDITY:
            r.readRelativeHumidity(v);
            strcpy(name, _("humidity")); sprintf(buf, "%.0f %%", v);
            break;
          case LPP_BAROMETRIC_PRESSURE:
            r.readPressure(v);
            strcpy(name, _("pressure")); sprintf(buf, _("%.0f hPa"), v);
            break;
          case LPP_ALTITUDE:
            r.readAltitude(v);
            strcpy(name, _("altitude")); sprintf(buf, _("%.0f m"), v);
            break;
          case LPP_POWER:
            r.readPower(v);
            strcpy(name, _("power")); sprintf(buf, _("%5.2f W"), v);
            break;
          default:
            r.skipData(type);
            strcpy(name, "?"); sprintf(buf, "");
        }
        display.setColor(UIColor::secondary_txt);
        if (padded) {
          drawClipped(0, y, row_right - inset - display.getTextWidth(buf) - column_gap, name);
        } else {
          drawLeft(0, y, name);
        }
        display.setColor(UIColor::primary_txt);
        drawRight(row_right, y, buf);
        y = y + 12;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset+1)%sensors_nb;
      else sensors_scroll_offset = 0;
      if (sensors_nb == 0) {
        display.setColor(UIColor::secondary_txt);
        drawCentered(display.width() / 2, 30, _("no sensors"));
      }
#endif
    } else if (_page == HomePage::CLOCK_NODES) {
      display.setTextSize(1);
      int used = usedClockNodes();
      if (used == 0) {
        display.setColor(UIColor::primary_txt);
        drawLeft(0, 22, _("no nodes set"));
      } else {
        display.setColor(UIColor::primary_txt);
        char name[24];
        int row = 0;
        for (int i = 0; i < CLOCK_NODE_MAX && row < 3; i++) {
          if (!clockNodeSlotUsed(_node_prefs->clock_nodes[i])) continue;
          the_mesh.lookupClockNodeName(_node_prefs->clock_nodes[i], name, sizeof(name));
          drawLeft(0, 18 + row * 12, name[0] ? name : _("(no name)"));
          row++;
        }
        display.setColor(UIColor::secondary_txt);
        if (used > 3) {
          char more[16];
          snprintf(more, sizeof(more), _("+%d more"), used - 3);
          drawRight(display.width() - 1, 54, more);
        }
      }
    } else if (_page == HomePage::NODE_PICKER) {
      display.setTextSize(1);
      int total = the_mesh.getNumContacts();
      if (total <= 0) {
        display.setColor(UIColor::secondary_txt);
        drawCentered(display.width() / 2, 26, _("no contacts"));
      } else {
        if (_pick_idx >= (uint32_t)total) _pick_idx = 0;
        int maxrows = 3;
        int start = (int)_pick_idx;
        if (start + maxrows > total) start = (total > maxrows) ? total - maxrows : 0;
        for (int r = 0; r < maxrows && start + r < total; r++) {
          int idx = start + r;
          ContactInfo c;
          if (!the_mesh.getContactByIdx((uint32_t)idx, c)) continue;
          bool sel = ((uint32_t)idx == _pick_idx);
          display.setColor(sel ? UIColor::warning_txt : UIColor::primary_txt);
          if (sel) drawLeft(0, 18 + r * 12, ">");
          drawLeft(10, 18 + r * 12, c.name[0] ? c.name : _("(no name)"));
        }
        display.setColor(UIColor::secondary_txt);
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "%d/%d", (int)_pick_idx + 1, total);
        drawRight(display.width() - 1, 54, cnt);
        drawLeft(0, 54, _("hold: add"));
      }
    } else if (_page == HomePage::TIME) {
      display.setTextSize(1);
      int y = 18;
      char tbuf[40];
      uint32_t now = _rtc->getCurrentTime();
      int16_t tz = _node_prefs ? _node_prefs->tz_offset_mins : 0;
      bool valid = now >= 1000000000UL;
      display.setColor(valid ? UIColor::primary_txt : UIColor::warning_txt);
      if (valid) {
        char hm[8], ds[16];
        formatClockHm(now, tz, hm, sizeof(hm));
        formatClockDate(now, tz, _node_prefs ? _node_prefs->ui_lang : UI_LANG_RU, ds, sizeof(ds));
        snprintf(tbuf, sizeof(tbuf), "%s  %s", hm, ds);
      } else {
        snprintf(tbuf, sizeof(tbuf), _("time not set"));
      }
      drawLeft(0, y, tbuf);
      y += 12;
      display.setColor(UIColor::primary_txt);
      snprintf(tbuf, sizeof(tbuf), _("tz: %+d h"), (int)(tz / 60));
      drawLeft(0, y, tbuf);
      drawRight(display.width() - 1, y,
        (_node_prefs && _node_prefs->ui_lang == 0) ? _("lang: RU") : _("lang: EN"));
      y += 12;
      display.setColor(UIColor::secondary_txt);
      drawLeft(0, y, valid ? _("source: set") : _("source: none"));
    } else if (_page == HomePage::SETUP) {
      uint8_t mode = _node_prefs ? _node_prefs->eink_refresh : EINK_REFRESH_CLEAR;
      if (mode >= EINK_REFRESH_COUNT) mode = EINK_REFRESH_CLEAR;
      bool clock_on = _node_prefs && _node_prefs->clock_saver != 0;
      uint8_t off_idx = _node_prefs ? _node_prefs->screen_off_idx : 3;
      char mode_txt[8];
      char off_txt[12];
      snprintf(mode_txt, sizeof(mode_txt), "%u", (unsigned)mode + 1);
      uint16_t secs = screenOffSeconds(off_idx);
      if (secs == 0) snprintf(off_txt, sizeof(off_txt), _("off"));
      else snprintf(off_txt, sizeof(off_txt), _("%u s"), (unsigned)secs);
      const char* rows_l[4] = { "Clock", "ScreenMode", _("Language"), _("Timeout") };
      const char* rows_r[4] = { clock_on ? _("on") : _("off"), mode_txt,
                                (_node_prefs && _node_prefs->ui_lang == UI_LANG_EN) ? "EN" : "RU",
                                off_txt };
      int nrows = setupRowCount();
      if (!display.isEink()) {
        // OLED has no ScreenMode row: shift the language row up into slot 1
        rows_l[1] = _("Language");
        rows_r[1] = rows_r[2];
        rows_l[2] = _("Timeout");
        rows_r[2] = off_txt;
      }
      display.setTextSize(1);
      for (int i = 0; i < nrows; i++) {
        int y = 20 + i * 12;
        display.setColor(UIColor::primary_txt);
        if (_setup_edit && (uint8_t)i == _setup_row) drawLeft(0, y, ">");
        drawLeft(10, y, rows_l[i]);
        drawRight(display.width() - 1, y, rows_r[i]);
      }
      if (_setup_edit && display.height() >= 64 && nrows <= 3) {
#ifndef HELTEC_LORA_V4
        display.setColor(UIColor::secondary_txt);
        drawCentered(display.width() / 2, 56, _("3 clicks: change"));
#endif
      }
#ifdef HELTEC_LORA_V4
    } else if (_page == HomePage::BATTERY) {
      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);
      snprintf(tmp, sizeof(tmp), _("Voltage: %.3f V"), _task->getBattMilliVolts() / 1000.0f);
      drawLeft(0, 24, tmp);
      snprintf(tmp, sizeof(tmp), "ADC: %.3f", board.getAdcMultiplier());
      drawLeft(0, 42, tmp);
    } else if (_page == HomePage::BATTERY_CAL) {
      display.setTextSize(1);
      if (_adc_edit) {
        display.setColor(UIColor::primary_txt);
        drawLeft(0, 18, _("Multimeter"));
        snprintf(tmp, sizeof(tmp), _("%u.%03u V"),
                 (unsigned)(_adc_mv / 1000), (unsigned)(_adc_mv % 1000));
        drawLeft(0, 32, tmp);
        display.setColor(UIColor::secondary_txt);
        drawLeft(inset + (_adc_digit == 0 ? 0 : (_adc_digit + 1) * 6), 43, "^");
        const int help_y = textY(56);
        drawLeft(0, help_y, "1:+ 2:-");
        display.fillRect(57, help_y + 4, 7, 1);
        drawLeft(69, help_y, _("next"));
      } else if (_battery_confirm) {
        display.setColor(UIColor::primary_txt);
        drawLeft(0, 18, _("Confirm"));
        if (_battery_choice == 0) {
          drawLeft(0, 30, _("ref 4.200 V"));
        } else if (_battery_choice == 1) {
          snprintf(tmp, sizeof(tmp), _("ref %u.%03u V"),
                   (unsigned)(_adc_mv / 1000), (unsigned)(_adc_mv % 1000));
          drawLeft(0, 30, tmp);
        } else {
          drawLeft(0, 30, _("restore factory"));
        }
        display.setColor(_battery_apply ? UIColor::primary_txt : UIColor::warning_txt);
        drawLeft(4, 42, _battery_apply ? _("Cancel") : _("> Cancel"));
        display.setColor(_battery_apply ? UIColor::warning_txt : UIColor::primary_txt);
        drawLeft(4, 54, _battery_apply ? _("> Apply") : _("Apply"));
      } else {
        display.setColor(UIColor::primary_txt);
        drawLeft(0, 18, _("ADC calibration"));
        const char* items[3] = { _("Full 4.20 V"), _("Multimeter"), _("Reset ADC") };
        for (uint8_t i = 0; i < 3; i++) {
          bool sel = (i == _battery_choice);
          display.setColor(sel ? UIColor::warning_txt : UIColor::primary_txt);
          drawLeft(4, 30 + i * 11, sel ? ">" : " ");
          drawLeft(14, 30 + i * 11, items[i]);
        }
      }
#endif
    } else if (_page == HomePage::ABOUT) {
      display.setTextSize(1);
      display.setColor(UIColor::primary_txt);
      drawCentered(display.width() / 2, 18, FIRMWARE_VERSION);
      snprintf(tmp, sizeof(tmp), _("build: %s"), __DATE__);
      display.setColor(UIColor::secondary_txt);
      drawLeft(0, 30, tmp);
      display.setColor(UIColor::primary_txt);
      snprintf(tmp, sizeof(tmp), _("node %s"), _node_prefs ? _node_prefs->node_name : "?");
      drawLeft(0, 42, tmp);
      uint32_t up = millis() / 1000UL;
      snprintf(tmp, sizeof(tmp), _("uptime %luh %lum"), (unsigned long)(up / 3600), (unsigned long)((up / 60) % 60));
      display.setColor(UIColor::secondary_txt);
      drawLeft(0, 54, tmp);
    } else if (_page == HomePage::MENU) {
#ifdef HELTEC_LORA_V4
      if (!isInSub()) {
        display.setTextSize(1);
        display.setColor(UIColor::primary_txt);
        drawLeft(0, 18, _("Radio and RX"));
        drawLeft(0, 30, _("Links and power"));
        drawLeft(0, 42, _("GPS, beacon, display"));
        drawLeft(0, 54, _("Time and system"));
      } else {
#endif
      // Root 5: the only way into the settings sub-screens.
      // click moves the marker, hold opens the selected page.
      const uint8_t total = menuCount();
      if (_menu_index >= total) _menu_index = 0;
      const uint8_t rows = 4;
      if (_menu_index < _menu_scroll) _menu_scroll = _menu_index;
      if (_menu_index >= _menu_scroll + rows) _menu_scroll = _menu_index - rows + 1;
      display.setTextSize(1);
      for (uint8_t r = 0; r < rows; r++) {
        uint8_t idx = _menu_scroll + r;
        if (idx >= total) break;
        uint8_t page = menuPageAt(idx);
        bool sel = idx == _menu_index;
        int y = 18 + r * (padded ? 11 : 12);
        if (sel) {
          display.setColor(UIColor::primary_txt);
          display.fillRect(0, y - 1, display.width(), 11);
        }
        display.setColor(sel ? UIColor::window_bkg : UIColor::primary_txt);
        const bool more = r == rows - 1 && _menu_scroll + rows < total;
        const int width = display.width() - 8 - (padded && more ? display.getTextWidth("v") + column_gap : 0);
        drawClipped(4, y, width, menuLabel(page));
      }
      if (_menu_scroll + rows < total) {
        display.setColor((_menu_index == _menu_scroll + rows - 1) ? UIColor::window_bkg : UIColor::secondary_txt);
        drawRight(row_right, 18 + (rows - 1) * (padded ? 11 : 12), "v");
      }
#ifdef HELTEC_LORA_V4
      }
#endif
#ifdef HELTEC_LORA_V4
    } else if (_page == HomePage::LANG) {
      display.setTextSize(1);
      uint8_t cur = _node_prefs ? _node_prefs->ui_lang : UI_LANG_EN;
      for (uint8_t i = 0; i < 2; i++) {
        bool sel = i == _lang_sel;
        int y = 20 + i * 12;
        if (sel) {
          display.setColor(UIColor::primary_txt);
          display.fillRect(0, y - 1, display.width(), 11);
        }
        display.setColor(sel ? UIColor::window_bkg : UIColor::primary_txt);
        drawClipped(4, y, display.width() - 20, i == UI_LANG_RU ? _("Russian") : "English");
        if (i == cur) {
          display.setColor(sel ? UIColor::window_bkg : UIColor::secondary_txt);
          drawRight(display.width() - 4, y, "*");
        }
      }
#endif
    } else if (_page == HomePage::SHUTDOWN) {
      display.setTextSize(1);
      if (_shutdown_init) {
        display.setColor(UIColor::warning_txt);
        drawCentered(display.width() / 2, 34, _("powering off"));
      } else {
        display.setColor(UIColor::primary_txt);
        drawLeft(0, 20, _("Power off?"));
        // Cancel is preselected: a stray long press must not power the board off.
        display.setColor(_shutdown_apply ? UIColor::primary_txt : UIColor::warning_txt);
        drawLeft(4, 34, _shutdown_apply ? _("Cancel") : _("> Cancel"));
        display.setColor(_shutdown_apply ? UIColor::warning_txt : UIColor::primary_txt);
        drawLeft(4, 46, _shutdown_apply ? _("> Apply") : _("Apply"));
      }
    }
    if (_act_open) {
      // Action list replaces the body while it is open; the header keeps the
      // screen name so it is obvious which screen the actions belong to.
      uint8_t total = actionCount();
      if (_act_idx >= total) _act_idx = 0;
      // wipe the body first: the screen render below already painted it
      display.setColor(UIColor::window_bkg);
      int body_top = 13;
#ifdef HELTEC_LORA_V4
      if (compact_header) body_top = OLED_HEADER_HEIGHT;
#endif
      display.fillRect(0, body_top, display.width(), display.height() - body_top);
      display.setTextSize(1);
      for (uint8_t i = 0; i < total && i < 4; i++) {
#ifdef HELTEC_LORA_V4
        drawV4ListRow(display, i, i == _act_idx, actionLabel(i));
#else
        bool sel = i == _act_idx;
        int y = 18 + i * 12;
        if (sel) {
          display.setColor(UIColor::primary_txt);
          display.fillRect(0, y - 1, display.width(), 11);
        }
        display.setColor(sel ? UIColor::window_bkg : UIColor::primary_txt);
        drawClipped(4, y, display.width() - 8, actionLabel(i));
#endif
      }
      return 2000;
    }
    if (_page == HomePage::SCAN) {
      return display.isEink() ? SCAN_REFRESH_MILLIS_EINK : SCAN_REFRESH_MILLIS;
    }
    if (_page == HomePage::FSCAN && fscan_state == FSCAN_RUN) {
      // e-ink skips the progress bar, finishing the scan requests a redraw
      return display.isEink() ? 5000 : FSCAN_REFRESH_MILLIS;
    }
    if (_page == HomePage::FIRST && !display.isEink() && _rtc->getCurrentTime() >= 1000000000UL) {
      uint32_t sec = _rtc->getCurrentTime() % 60UL;
      int wait = (int)((60UL - sec) * 1000UL);
      if (wait < 1000) wait = 1000;
      return wait;
    }
    if (_page == HomePage::NEIGHBORS) return 1000;
    if (_page == HomePage::RADIO && compact_header) return 2000;
#if ENV_INCLUDE_GPS == 1
    if (_page == HomePage::BEACON) return 1000;
#endif
    return 5000;   // next render after 5000 ms
  }

  bool handleInput(char c) override {
    setUiLang(_node_prefs ? _node_prefs->ui_lang : UI_LANG_EN);
#if defined(HELTEC_LORA_V4) && ENV_INCLUDE_GPS == 1
    if (isBeaconPicker()) {
      handleBeaconPicker(c);
      return true;
    }
#endif
    // A modal action list consumes every gesture before page shortcuts can run.
    if (_act_open) {
      uint8_t total = actionCount();
      if (c == KEY_NEXT || c == KEY_RIGHT) {
#ifdef HELTEC_LORA_V4
        if (_act_idx + 1 < total) _act_idx++;
#else
        if (total > 0) _act_idx = (uint8_t)((_act_idx + 1) % total);
#endif
      } else if (c == KEY_LEFT || c == KEY_PREV) {
#ifdef HELTEC_LORA_V4
        if (_act_idx > 0) _act_idx--;
        else if (_settings_action_page) { _settings_action_page = false; leaveSub(); }
        else _act_open = false;
#else
        _act_open = false;
#endif
      } else if (c == KEY_ENTER) {
        uint8_t idx = _act_idx;
        _act_open = false;
        runAction(idx);
#ifdef HELTEC_LORA_V4
        if (_settings_action_page && !_radio_confirm
#if ENV_INCLUDE_GPS == 1
            && !isBeaconPicker()
#endif
        ) {
          _settings_action_page = false;
          leaveSub();
        }
#endif
      }
      return true;
    }
    if (_radio_confirm && _page == HomePage::RADIO) {
      if (c == KEY_NEXT || c == KEY_RIGHT) {
#ifdef HELTEC_LORA_V4
        _radio_apply = true;
#else
        _radio_apply = !_radio_apply;
#endif
      }
      else if (c == KEY_LEFT || c == KEY_PREV) {
#ifdef HELTEC_LORA_V4
        if (_radio_apply) { _radio_apply = false; return true; }
#endif
        _radio_confirm = false;
#ifdef HELTEC_LORA_V4
        if (_settings_action_page) { _settings_action_page = false; leaveSub(); }
#endif
      }
      else if (c == KEY_ENTER) {
        if (_radio_apply) {
          if (radioNextProfile(the_mesh.radioProfile()) == _radio_target) _task->cycleRadioProfile();
          else _task->showAlert(_("radio changed"), 1200);
        }
        _radio_confirm = false;
#ifdef HELTEC_LORA_V4
        if (_settings_action_page) { _settings_action_page = false; leaveSub(); }
#endif
      }
      return true;
    }
#ifdef HELTEC_LORA_V4
    if (_power_confirm && _page == HomePage::POWER) {
      if (c == KEY_NEXT || c == KEY_RIGHT) _power_apply = true;
      else if (c == KEY_LEFT || c == KEY_PREV) {
        if (_power_apply) _power_apply = false; else _power_confirm = false;
      } else if (c == KEY_ENTER) {
        _power_confirm = false;
        if (_power_apply) {
          bool cpu_ok = _task->canSelectMcuSleep() && !_task->gpsBlocksCpuSleep() && !_task->bleBlocksCpuSleep();
          if ((_power_cpu && !cpu_ok) || (_power_lna && !_task->canControlFemLna()))
            _task->showAlert(_("state changed"), 1200);
          else _task->applyPowerProfile(_power_boost, _power_lna, _power_cpu);
        }
      }
      return true;
    }
    if (_scan_confirm && _page == HomePage::FSCAN) {
      if (c == KEY_NEXT || c == KEY_RIGHT) _scan_apply = true;
      else if (c == KEY_LEFT || c == KEY_PREV) {
        if (_scan_apply) _scan_apply = false; else _scan_confirm = false;
      } else if (c == KEY_ENTER) {
        _scan_confirm = false;
        if (_scan_apply) {
          _task->notify(UIEventType::ack);
          startFreqScan();
        }
      }
      return true;
    }
    if (_conn_confirm && _page == HomePage::CONNECTIONS) {
      if (c == KEY_NEXT || c == KEY_RIGHT) _conn_apply = true;
      else if (c == KEY_LEFT || c == KEY_PREV) {
        if (_conn_apply) _conn_apply = false; else _conn_confirm = false;
      } else if (c == KEY_ENTER) {
        _conn_confirm = false;
        bool enabled = _conn_selected == 0 ? _task->isBluetoothEnabled() :
                       _conn_selected == 1 ? _task->isWiFiEnabled() : _task->isUSBEnabled();
        if (_conn_apply && enabled != _conn_was_enabled) {
          _task->showAlert(_("state changed"), 1200);
        } else if (_conn_apply && _conn_selected == 0) {
          if (enabled) _task->disableBluetooth(); else _task->enableBluetooth();
        } else if (_conn_apply && _conn_selected == 1) {
          if (enabled) { _task->disableWiFi(); _task->disableWiFi2(); }
          else { _task->enableWiFi(); _task->enableWiFi2(); }
        } else if (_conn_apply) {
          if (enabled) _task->disableUSB(); else _task->enableUSB();
        }
      }
      return true;
    }
#endif
    if (c == KEY_QUAD) {
#if ENV_INCLUDE_GPS == 1 && !defined(HELTEC_LORA_V4)
      if (_page != HomePage::BEACON) return false;
      if (the_mesh.stopBeacon()) {
        _task->notify(UIEventType::ack);
        _task->showAlert("beacon off", 900);
      }
      return true;
#else
      return false;
#endif
    }
    if (c == KEY_SELECT) {
      if (_page == HomePage::RADIO) {
        _act_idx = 0;
        _act_open = true;
        return true;
      }
      if (_page == HomePage::SETUP && _setup_edit) {
        changeSetupValue();
        return true;
      }
#if ENV_INCLUDE_GPS == 1 && defined(HELTEC_LORA_V4)
      if (_page == HomePage::BEACON) return true;
#elif ENV_INCLUDE_GPS == 1
      if (_page == HomePage::BEACON && the_mesh.cycleBeaconInterval()) {
        char mode[20];
        char target[32];
        char pos[40];
        char sent[24];
        char alert[48];
        the_mesh.formatBeacon(mode, sizeof(mode), target, sizeof(target), pos, sizeof(pos), sent, sizeof(sent));
        snprintf(alert, sizeof(alert), "%s", mode);
        _task->notify(UIEventType::ack);
        _task->showAlert(alert, 900);
        return true;
      }
#endif
      _task->toggleBuzzer();
      return true;
    }
    if ((c == KEY_UP || c == KEY_DOWN) && _page == HomePage::SETUP && _setup_edit) {
      stepSetupRow(c == KEY_UP ? -1 : 1);
      return true;
    }
    // Screens with more than one action open the list instead of guessing.
    if (c == KEY_ENTER && actionCount() > 0) {
      _act_open = true;
      _act_idx = 0;
      return true;
    }
#ifdef HELTEC_LORA_V4
    if (_page == HomePage::LANG) {
      if (c == KEY_NEXT || c == KEY_RIGHT) {
        _lang_sel = UI_LANG_EN;
      } else if (c == KEY_LEFT || c == KEY_PREV) {
        if (_lang_sel == UI_LANG_EN) _lang_sel = UI_LANG_RU;
        else leaveSub();
      } else if (c == KEY_ENTER && _node_prefs) {
        _node_prefs->ui_lang = _lang_sel;
        setUiLang(_lang_sel);
        the_mesh.savePrefs();
        _task->notify(UIEventType::ack);
      }
      return true;
    }
#endif
    if (c == KEY_LEFT || c == KEY_PREV) {
#ifdef HELTEC_LORA_V4
      if (_page == HomePage::MENU && isInSub()) {
        if (_menu_index > 0) _menu_index--; else leaveSub();
        return true;
      }
      if (_page == HomePage::CONNECTIONS && isInSub() && _conn_selected > 0) {
        _conn_selected--;
        return true;
      }
      if (_page == HomePage::NODE_PICKER && _pick_idx > 0) {
        _pick_idx--;
        return true;
      }
      if (_page == HomePage::SHUTDOWN && _shutdown_apply) {
        _shutdown_apply = false;
        return true;
      }
      if (_page == HomePage::BATTERY_CAL) {
        if (_adc_edit) {
          static const uint16_t place[] = {1000, 100, 10, 1};
          uint16_t unit = place[_adc_digit];
          uint16_t digit = (_adc_mv / unit) % 10;
          _adc_mv = (uint16_t)(_adc_mv - digit * unit + ((digit + 9) % 10) * unit);
          return true;
        }
        if (_battery_confirm) {
          if (_battery_apply) _battery_apply = false; else _battery_confirm = false;
          return true;
        }
        if (_battery_choice > 0) { _battery_choice--; return true; }
        _page = HomePage::BATTERY;
        return true;
      }
#endif
      if (_page == HomePage::FSCAN) {
#ifdef HELTEC_LORA_V4
        if (fscan_state == FSCAN_RUN) { stopFreqScan(); return true; }
#endif
        stopFreqScan();
      }
      if (_page == HomePage::SETUP && _setup_edit) {
#ifdef HELTEC_LORA_V4
        if (_setup_row > 0) _setup_row--; else _setup_edit = false;
#else
        _setup_edit = false;
#endif
        return true;
      }
      if (isInSub()) {
        leaveSub();
        _setup_edit = false;
        return true;
      }
      _page = stepPage(-1);
      if (_page == HomePage::SETUP) {
        _setup_row = 0;
        _setup_edit = false;
      }
      if (_page == HomePage::NEIGHBORS) {
        neighbors_scroll_offset = 0;
        neighbors_next_scroll = 0;
      }
      return true;
    }
    // Экран «Выключить»: click switches between Отмена and Применить.
    if (_page == HomePage::SHUTDOWN && (c == KEY_NEXT || c == KEY_RIGHT)) {
#ifdef HELTEC_LORA_V4
      _shutdown_apply = true;
#else
      _shutdown_apply = !_shutdown_apply;
#endif
      return true;
    }
    // МЕНЮ: click moves the marker, hold opens the selected sub-screen.
    if (_page == HomePage::MENU && (c == KEY_NEXT || c == KEY_RIGHT)
#ifdef HELTEC_LORA_V4
        && isInSub()
#endif
        ) {
      uint8_t total = menuCount();
#ifdef HELTEC_LORA_V4
      if (_menu_index + 1 < total) _menu_index++;
#else
      if (total > 0) _menu_index = (_menu_index + 1) % total;
#endif
      return true;
    }
    // CONNECTIONS: click selects, double click exits, hold toggles selection.
    if (_page == HomePage::CONNECTIONS && (c == KEY_NEXT || c == KEY_RIGHT)) {
#ifdef HELTEC_LORA_V4
      if (_conn_selected < 2) _conn_selected++;
#else
      _conn_selected = (_conn_selected + 1) % 3;
#endif
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      if (_page == HomePage::RADIO
#ifdef HELTEC_LORA_V4
          && isInSub()
#endif
          ) {
        _radio_details = !_radio_details;
        return true;
      }
#ifdef HELTEC_LORA_V4
      if (_page == HomePage::BATTERY_CAL) {
        if (_adc_edit) {
          static const uint16_t place[] = {1000, 100, 10, 1};
          uint16_t unit = place[_adc_digit];
          uint16_t digit = (_adc_mv / unit) % 10;
          _adc_mv = (uint16_t)(_adc_mv - digit * unit + ((digit + 1) % 10) * unit);
        } else if (_battery_confirm) {
          _battery_apply = true;
        } else {
          if (_battery_choice < 2) _battery_choice++;
        }
        return true;
      }
#endif
      if (_page == HomePage::NODE_PICKER) {
        int total = the_mesh.getNumContacts();
#ifdef HELTEC_LORA_V4
        if (_pick_idx + 1 < (uint32_t)total) _pick_idx++;
#else
        if (total > 0) _pick_idx = (uint32_t)((_pick_idx + 1) % (uint32_t)total);
#endif
        return true;
      }
      if (_page == HomePage::FSCAN && !isInSub()) stopFreqScan();
      if (_page == HomePage::SETUP && _setup_edit) {
#ifdef HELTEC_LORA_V4
        if (_setup_row + 1 < setupRowCount()) _setup_row++;
#else
        stepSetupRow(1);
#endif
        return true;
      }
      if (isInSub()) return true;   // sub-screens stay put; double click goes back
      _page = stepPage(1);
      if (_page == HomePage::SETUP) {
        _setup_row = 0;
        _setup_edit = false;
      }
      if (_page == HomePage::NEIGHBORS) {
        neighbors_scroll_offset = 0;
        neighbors_next_scroll = 0;
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::FSCAN) {
      if (fscan_state != FSCAN_RUN) {
#ifdef HELTEC_LORA_V4
        _scan_apply = false;
        _scan_confirm = true;
#else
        _task->notify(UIEventType::ack);
        startFreqScan();
        _task->showAlert("Scanning...", 600);
#endif
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::SCAN) {
      scan_peak = -127;
      scan_sum = 0;
      scan_cnt = 0;
      _task->showAlert("Peak reset", 600);
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::NEIGHBORS) {
      neighbors_scroll_offset = 0;
      neighbors_next_scroll = 0;
      _task->notify(UIEventType::ack);
      if (the_mesh.sendNeighborDiscover()) {
        _task->showAlert("Polling...", 1000);
      } else {
        _task->showAlert("Poll failed", 1000);
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::RADIO) {
      // Long press toggles the FEM LNA, but only where the board actually has one.
      // On boards without it (Heltec V4) stay silent instead of showing "LNA: n/a".
      if (_task->canControlFemLna()) {
        _task->toggleFemLna();
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::POWER) {
      bool fem = _task->canControlFemLna();
      bool cpu_ok = _task->canSelectMcuSleep() && !_task->gpsBlocksCpuSleep() && !_task->bleBlocksCpuSleep();
      bool boost = _node_prefs->rx_boosted_gain != 0;
      bool lna = fem && _node_prefs->radio_fem_rxgain != 0;
      bool cpu = cpu_ok && _node_prefs->mcu_sleep != 0;
      PowerChoice choices[6];
      int n = buildPowerChoices(fem, cpu_ok, choices);
      int cur = -1;
      for (int i = 0; i < n; i++) {
        if (choices[i].boost == boost && choices[i].lna == lna && choices[i].cpu == cpu) {
          cur = i;
          break;
        }
      }
      int next = (cur < 0) ? 0 : (cur + 1) % n;
      _task->applyPowerProfile(choices[next].boost, choices[next].lna, choices[next].cpu);
      return true;
    }
#ifdef HELTEC_LORA_V4
    if (c == KEY_ENTER && _page == HomePage::BATTERY) {
      _page = HomePage::BATTERY_CAL;
      _battery_choice = 0;
      _battery_confirm = false;
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::BATTERY_CAL) {
      if (_adc_edit) {
        if (_adc_digit < 3) { _adc_digit++; return true; }
        if (_adc_mv < 3000 || _adc_mv > 4250) {
          _task->showAlert(_("ADC ref 3.00-4.25V"), 1600);
          _adc_digit = 0;
          return true;
        }
        _adc_edit = false;
        _battery_confirm = true;
        _battery_apply = false;
        return true;
      }
      if (!_battery_confirm) {
        if (_battery_choice == 1) {
          uint16_t mv = _task->getBattMilliVolts();
          if (mv < 3000 || mv > 4250) mv = 4150;
          _adc_mv = mv;
          _adc_digit = 0;
          _adc_edit = true;
          return true;
        }
        _battery_confirm = true;
        _battery_apply = false;
        return true;
      }
      if (_battery_apply) {
        char result[160] = {0};
        if (_battery_choice == 1) the_mesh.calibrateBatteryTo(_adc_mv, result);
        else the_mesh.calibrateBatteryFromUI(_battery_choice == 0, result);
        _task->showAlert(strncmp(result, "OK", 2) == 0 ? _("ADC saved") : _("ADC not saved"), 1600);
      }
      _battery_confirm = false;
      _page = HomePage::BATTERY;
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      if (_task->isBluetoothEnabled()) {  // toggle Bluetooth on/off
        _task->disableBluetooth();
      } else {
        _task->enableBluetooth();
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::CONNECTIONS) {
#ifdef HELTEC_LORA_V4
      _conn_was_enabled = _conn_selected == 0 ? _task->isBluetoothEnabled() :
                          _conn_selected == 1 ? _task->isWiFiEnabled() : _task->isUSBEnabled();
      _conn_apply = false;
      _conn_confirm = true;
#else
      if (_conn_selected == 0) {
        if (_task->isBluetoothEnabled()) _task->disableBluetooth(); else _task->enableBluetooth();
      } else if (_conn_selected == 1) {
        if (_task->isWiFiEnabled()) { _task->disableWiFi(); _task->disableWiFi2(); }
        else { _task->enableWiFi(); _task->enableWiFi2(); }
      } else {
        if (_task->isUSBEnabled()) _task->disableUSB(); else _task->enableUSB();
      }
#endif
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::RECENT) {
      _task->notify(UIEventType::ack);
      if (the_mesh.advert()) {
        _task->showAlert("Advert sent!", 1000);
      } else {
        _task->showAlert("Advert failed..", 1000);
      }
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#ifndef HELTEC_LORA_V4
    if (c == KEY_ENTER && _page == HomePage::BEACON) {
      the_mesh.cycleBeacon();
      char mode[20];
      char target[32];
      char pos[40];
      char sent[24];
      char alert[48];
      the_mesh.formatBeacon(mode, sizeof(mode), target, sizeof(target), pos, sizeof(pos), sent, sizeof(sent));
      snprintf(alert, sizeof(alert), "%s", mode);
      if (target[0]) {
        size_t used = strlen(alert);
        if (used + 2 < sizeof(alert)) {
          alert[used] = ' ';
          alert[used + 1] = 0;
          strncat(alert, target, sizeof(alert) - used - 2);
        }
      }
      _task->notify(UIEventType::ack);
      _task->showAlert(alert, 900);
      return true;
    }
#endif
#endif
#if UI_SENSORS_PAGE == 1
    if (c == KEY_ENTER && _page == HomePage::SENSORS) {
      // read the sensors right now instead of waiting for the 5 s timer
      sensors_lpp.reset();
      next_sensors_refresh = 0;
      sensors_scroll_offset = 0;
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::SETUP) {
#ifdef HELTEC_LORA_V4
      if (_setup_edit) changeSetupValue(); else { _setup_row = 0; _setup_edit = true; }
#else
      _setup_edit = !_setup_edit;
#endif
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN && !_shutdown_apply) {
      // _("Cancel") was chosen: leave the dialog, the board stays on
      leaveSub();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::TIME) {
      bool ok = the_mesh.pullClock();
      _task->showAlert(ok ? _("time request") : _("no one to ask"), 1200);
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;  // need to wait for button to be released
      return true;
    }
    // long press on the main page reopens messages already previewed or dismissed
    if (c == KEY_ENTER && (_page == HomePage::FIRST || _page == HomePage::MESSAGES)) {
      return _task->reopenMsgPreview();
    }
    // МЕНЮ: hold opens the highlighted sub-screen
    if (c == KEY_ENTER && _page == HomePage::MENU) {
#ifdef HELTEC_LORA_V4
      if (!isInSub()) {
        _menu_index = 0;
        _menu_scroll = 0;
        enterSub(HomePage::MENU);
        return true;
      }
#endif
      if (_menu_index >= menuCount()) _menu_index = 0;
      uint8_t target = menuPageAt(_menu_index);
      // Every dialog opens with its safe choice preselected.
      if (target == HomePage::SHUTDOWN) _shutdown_apply = false;
#ifdef HELTEC_LORA_V4
      if (target == HomePage::LANG) _lang_sel = _node_prefs && _node_prefs->ui_lang == UI_LANG_RU ? UI_LANG_RU : UI_LANG_EN;
#endif
      enterSub(target);
#ifdef HELTEC_LORA_V4
      if (target == HomePage::RADIO
#if ENV_INCLUDE_GPS == 1
          || target == HomePage::GPS || target == HomePage::BEACON
#endif
          ) {
        _settings_action_page = true;
        _act_idx = 0;
        _act_open = true;
      }
#endif
      return true;
    }
    return false;
  }
};

#ifndef UI_MSG_PREVIEW_SIZE
  #define UI_MSG_PREVIEW_SIZE 78
#endif

#ifdef HELTEC_LORA_V4
class MsgPreviewScreen : public UIScreen {
  static constexpr int HISTORY_SIZE = 32;
  struct Message {
    uint32_t timestamp;
    char sender[32];
    char text[MAX_TEXT_LEN + 1];
    bool group;
    bool unread;
  };
  enum View { LIST, MESSAGE, ACTIONS, INFO, REMOVE };
  UITask* _task;
  mesh::RTCClock* _rtc;
  Message _messages[HISTORY_SIZE];
  int _size = 0;
  int _newest = HISTORY_SIZE - 1;
  int _selected = _newest;
  int _groups = 0;
  int _chats = 0;
  View _view = LIST;
  int _row = 0;
  int _page = 0;
  int _next_offset = 0;
  // Each page consumes at least one byte from the filtered message.
  uint16_t _offsets[MAX_TEXT_LEN + 1] = {};

  int slotAt(int index) const { return (_newest + HISTORY_SIZE - index) % HISTORY_SIZE; }
  int selectedIndex() const { return (_newest + HISTORY_SIZE - _selected) % HISTORY_SIZE; }

  void openMessage(int slot) {
    _selected = slot;
    _page = 0;
    _next_offset = 0;
    _view = MESSAGE;
  }

  void markRead(Message& msg) {
    if (!msg.unread) return;
    msg.unread = false;
    if (msg.group) _groups--; else _chats--;
  }

  void removeSelected() {
    int index = selectedIndex();
    markRead(_messages[_selected]);
    // Close the gap on the newer side, keeping older entries in place.
    for (int i = index; i > 0; i--) _messages[slotAt(i)] = _messages[slotAt(i - 1)];
    _newest = slotAt(1);
    _size--;
    _selected = slotAt(index < _size ? index : (_size > 0 ? _size - 1 : 0));
    _view = LIST;
  }

  void formatAge(const Message& msg, char* buf, size_t size) const {
    uint32_t now = _rtc->getCurrentTime();
    unsigned secs = now >= msg.timestamp ? now - msg.timestamp : 0;
    if (secs < 60) snprintf(buf, size, _("%u s"), secs);
    else if (secs < 3600) snprintf(buf, size, _("%u min"), secs / 60);
    else snprintf(buf, size, _("%u h"), secs / 3600);
  }

  void renderHeader(DisplayDriver& display, const char* title, const char* page = "") {
    HomeScreen::renderV4Header(display, *_task, title, true, page);
    display.setColor(UIColor::primary_txt);
  }

public:
  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc) : _task(task), _rtc(rtc) {}

  int groupCount() const { return _groups; }
  int chatCount() const { return _chats; }
  int unreadCount() const { return _groups + _chats; }
  int storedCount() const { return _size; }

  bool reopen() {
    _view = LIST;
    _selected = _newest;
    return true;
  }

  void addPreview(uint8_t, const char* from_name, const char* text, bool group = false) {
    _newest = (_newest + 1) % HISTORY_SIZE;
    auto& msg = _messages[_newest];
    if (_size == HISTORY_SIZE) {
      markRead(msg);
      if (_selected == _newest) {
        _selected = slotAt(HISTORY_SIZE - 1);
        _view = LIST;
      }
    } else {
      _size++;
    }
    msg.timestamp = _rtc->getCurrentTime();
    StrHelper::strncpy(msg.sender, from_name, sizeof(msg.sender));
    StrHelper::strncpy(msg.text, text, sizeof(msg.text));
    msg.group = group;
    msg.unread = true;
    if (group) _groups++; else _chats++;
    if (_size == 1) _selected = _newest;
  }

  int render(DisplayDriver& display) override {
    setUiLang(the_mesh.getNodePrefs()->ui_lang);
    if (_view == LIST || _size == 0) {
      renderHeader(display, _("Messages"));
      if (_size == 0) display.drawTextLeftAlign(4, 18, _("No messages"));
      int sel = selectedIndex();
      int first = sel >= 4 ? sel - 3 : 0;
      for (int i = first; i < _size && i < first + 4; i++) {
        const auto& msg = _messages[slotAt(i)];
        char age[20], right[24];
        formatAge(msg, age, sizeof(age));
        snprintf(right, sizeof(right), "%s%s", msg.unread ? "*" : "", age);
        drawV4ListRow(display, i - first, i == sel, msg.sender, right);
      }
      return 1000;
    }

    const auto& msg = _messages[_selected];
    if (_view == MESSAGE) {
      char filtered[sizeof(msg.text)];
      display.translateUTF8ToBlocks(filtered, msg.text, sizeof(filtered));
      display.setTextSize(1);
      display.setColor(UIColor::primary_txt);
      display.setCursor(OLED_INSET, 18);
      const char* start = filtered + _offsets[_page];
      const char* rest = display.printWordWrap(start, display.width() - 2 * OLED_INSET);
      _next_offset = *rest && rest > start ? rest - filtered : 0;
      char page[8] = "";
      if (_page > 0 || _next_offset) snprintf(page, sizeof(page), "%u%s", (unsigned)(_page + 1), _next_offset ? "+" : "");
      renderHeader(display, msg.sender, page);
    } else if (_view == ACTIONS) {
      renderHeader(display, _("Message"));
      const char* actions[] = {_("Next message"), _("Mark as read"), _("Information"), _("Remove from view")};
      for (int i = 0; i < 4; i++) drawV4ListRow(display, i, _row == i, actions[i]);
    } else if (_view == INFO) {
      renderHeader(display, _("Message"));
      char age[20], label[64];
      snprintf(label, sizeof(label), "%s %s", msg.group ? "#" : "@", msg.sender);
      display.drawTextEllipsized(4, 18, display.width() - 8, label);
      formatAge(msg, age, sizeof(age));
      snprintf(label, sizeof(label), _("Received: %s"), age);
      display.drawTextEllipsized(4, 30, display.width() - 8, label);
      snprintf(label, sizeof(label), _("To phone: %d"), _task->getMsgCount());
      display.drawTextEllipsized(4, 42, display.width() - 8, label);
    } else {
      renderHeader(display, _("Message"));
      display.drawTextLeftAlign(4, 18, _("Remove this copy?"));
      drawV4ListRow(display, 1, _row == 0, _("Cancel"));
      drawV4ListRow(display, 2, _row == 1, _("Remove"));
    }
    return 1000;
  }

  bool handleInput(char c) override {
    bool next = c == KEY_NEXT || c == KEY_RIGHT;
    bool prev = c == KEY_PREV || c == KEY_LEFT;
    if (_view == LIST) {
      int index = selectedIndex();
      if (next && index + 1 < _size) _selected = slotAt(index + 1);
      else if (prev) {
        if (_size > 0 && index > 0) _selected = slotAt(index - 1);
        else _task->gotoHomeScreen();
      } else if (c == KEY_ENTER && _size > 0) openMessage(_selected);
    } else if (_view == MESSAGE) {
      if (next && _next_offset) {
        _offsets[++_page] = _next_offset;
        _next_offset = 0;
      } else if (prev) {
        _next_offset = 0;
        if (_page > 0) _page--; else _view = LIST;
      } else if (c == KEY_ENTER) {
        _row = 0;
        _view = ACTIONS;
      }
    } else if (_view == ACTIONS) {
      if (next && _row < 3) _row++;
      else if (prev) {
        if (_row > 0) _row--; else _view = MESSAGE;
      } else if (c == KEY_ENTER) {
        if (_row == 0) {
          int index = selectedIndex() + 1;
          if (index < _size) openMessage(slotAt(index)); else _view = LIST;
        } else if (_row == 1) {
          markRead(_messages[_selected]);
          _view = MESSAGE;
        } else if (_row == 2) {
          _view = INFO;
        } else {
          _row = 0;
          _view = REMOVE;
        }
      }
    } else if (_view == INFO) {
      if (prev || c == KEY_ENTER) _view = ACTIONS;
    } else {
      if (next) _row = 1;
      else if (prev && _row > 0) _row = 0;
      else if (c == KEY_ENTER && _row == 1) removeSelected();
      else if (prev || c == KEY_ENTER) {
        _row = 3;
        _view = ACTIONS;
      }
    }
    return true;
  }
};
#else
class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  struct MsgEntry {
    uint32_t timestamp;
    char origin[62];
    char msg[MAX_TEXT_LEN + 1];
    bool group;
  };
  #define MAX_UNREAD_MSGS   32
  int num_unread;
  int stored = 0;   // messages still held in the ring after they are read or dismissed
  int groups = 0;
  int chats = 0;
  int head = MAX_UNREAD_MSGS - 1;   // message currently on screen
  int latest = MAX_UNREAD_MSGS - 1; // most recently received message
  bool replay = false;
  MsgEntry unread[MAX_UNREAD_MSGS];

  // long messages are split into screen-sized pages that rotate automatically
  int page_start = 0;   // offset into the filtered text of the page on screen
  int page_next = 0;    // offset of the following page, 0 when the text ends on this page
  int page_num = 0;
  int shown_head = -1;  // last message/page actually drawn; -1 forces a full refresh
  int shown_page = -1;
  unsigned long page_until = 0;

  void resetPage() {
    page_start = page_next = page_num = 0;
    page_until = millis() + MSG_PAGE_MILLIS;
  }

  void nextPage() {
    if (page_next > 0) {
      page_start = page_next;
      page_num++;
    } else {
      page_start = page_num = 0;
    }
    page_until = millis() + MSG_PAGE_MILLIS;
  }

  int oldestIndex() const {
    if (stored <= 0) return latest;
    return (latest + MAX_UNREAD_MSGS - (stored - 1)) % MAX_UNREAD_MSGS;
  }

  // oldest message that is still counted as unread
  int oldestUnread() const {
    if (num_unread <= 0) return latest;
    return (latest + MAX_UNREAD_MSGS - (num_unread - 1)) % MAX_UNREAD_MSGS;
  }

  void dropHead() {
    if (stored <= 0) return;
    int oldest = oldestIndex();
    int pos = 0;
    for (int idx = oldest; idx != head && pos < stored; pos++) {
      idx = (idx + 1) % MAX_UNREAD_MSGS;
    }
    if (pos >= stored) return;

    bool was_group = unread[head].group;
    for (int i = pos; i < stored - 1; i++) {
      int dst = (oldest + i) % MAX_UNREAD_MSGS;
      unread[dst] = unread[(dst + 1) % MAX_UNREAD_MSGS];
    }
    stored--;
    if (num_unread > 0) num_unread--;
    if (was_group) { if (groups > 0) groups--; }
    else if (chats > 0) chats--;

    if (stored == 0 || num_unread == 0) {
      num_unread = 0;
      if (stored == 0) groups = chats = 0;
      _task->gotoHomeScreen();
      return;
    }
    latest = (oldest + stored - 1) % MAX_UNREAD_MSGS;
    head = (pos >= stored) ? latest : (oldest + pos) % MAX_UNREAD_MSGS;
    resetPage();
  }

public:
  // The phone took the oldest message that was also shown here. Remove that copy.
  bool dropOldest() {
    if (stored <= 0) return true;
    int oldest = oldestIndex();
    bool viewing = (head == oldest);
    bool was_group = unread[oldest].group;
    bool counted = (num_unread > 0 && num_unread == stored);

    for (int i = 0; i < stored - 1; i++) {
      int dst = (oldest + i) % MAX_UNREAD_MSGS;
      unread[dst] = unread[(dst + 1) % MAX_UNREAD_MSGS];
    }
    stored--;
    if (counted) {
      num_unread--;
      if (was_group) { if (groups > 0) groups--; }
      else if (chats > 0) chats--;
    }
    if (stored == 0) {
      num_unread = 0;
      groups = chats = 0;
      return true;
    }
    latest = (oldest + stored - 1) % MAX_UNREAD_MSGS;
    if (viewing) {
      head = oldest;
      resetPage();
    } else {
      head = (head + MAX_UNREAD_MSGS - 1) % MAX_UNREAD_MSGS;
    }
    return false;
  }

  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc) : _task(task), _rtc(rtc) { num_unread = 0; }

  int groupCount() const { return groups; }
  int chatCount() const { return chats; }
  int unreadCount() const { return num_unread; }
  int storedCount() const { return stored; }

  bool reopen() {
    if (stored == 0) return false;
    num_unread = stored;
    head = oldestUnread();
    groups = chats = 0;
    for (int i = 0; i < stored; i++) {
      int idx = (latest + MAX_UNREAD_MSGS - i) % MAX_UNREAD_MSGS;
      if (unread[idx].group) groups++;
      else chats++;
    }
    replay = true;
    resetPage();
    return true;
  }

  void addPreview(uint8_t path_len, const char* from_name, const char* msg, bool group = false) {
    int prev_unread = num_unread;
    int slot = (latest + 1) % MAX_UNREAD_MSGS;
    bool full = stored == MAX_UNREAD_MSGS;
    bool head_overwritten = full && head == slot;
    if (full && num_unread == MAX_UNREAD_MSGS) {
      if (unread[slot].group) { if (groups > 0) groups--; }
      else if (chats > 0) chats--;
    } else if (!full) {
      stored++;
      num_unread++;
    } else if (num_unread < MAX_UNREAD_MSGS) {
      num_unread++;
    }
    latest = slot;
    replay = false;

    auto p = &unread[latest];
    p->group = group;
    if (group) groups++; else chats++;
    p->timestamp = _rtc->getCurrentTime();
    if (path_len == 0xFF) {
      sprintf(p->origin, "(D) %s:", from_name);
    } else {
      sprintf(p->origin, "(%d) %s:", (uint32_t) path_len, from_name);
    }
    StrHelper::strncpy(p->msg, msg, sizeof(p->msg));
    // start at the oldest unread; a message that arrives mid-read waits at the end
    if (prev_unread == 0 || head_overwritten) {
      head = oldestUnread();
      resetPage();
    }
  }

  int render(DisplayDriver& display) override {
    auto p = &unread[head];
    char filtered_msg[sizeof(p->msg)];
    display.translateUTF8ToBlocks(filtered_msg, p->msg, sizeof(filtered_msg));
    int msg_len = strlen(filtered_msg);

    if (page_next > 0 || page_num > 0) {
      if ((long)(millis() - page_until) >= 0) nextPage();
    }
    if (page_start >= msg_len) resetPage();

    // short screens (128x64 OLED and alike) merge the sender into the title row to fit one more text line
    bool compact = display.height() < 100;
    int text_y = compact ? 14 : 25;

    display.setTextSize(1);
    display.setCursor(0, text_y);
    display.setColor(UIColor::primary_txt);
    const char* rest = display.printWordWrap(&filtered_msg[page_start], display.width());
    page_next = (*rest && rest > &filtered_msg[page_start]) ? (int)(rest - filtered_msg) : 0;
    bool paged = page_next > 0 || page_num > 0;

    char tmp[24];
    char age[8];
    int secs = _rtc->getCurrentTime() - p->timestamp;
    if (secs < 60) {
      sprintf(age, "%ds", secs);
    } else if (secs < 60*60) {
      sprintf(age, "%dm", secs / 60);
    } else {
      sprintf(age, "%dh", secs / (60*60));
    }
    char page_tag[8] = "";
    if (paged) sprintf(page_tag, "p%d%s ", page_num + 1, page_next > 0 ? "+" : "");

    char filtered_origin[sizeof(p->origin)];
    display.translateUTF8ToBlocks(filtered_origin, p->origin, sizeof(filtered_origin));

    if (compact) {
      if (num_unread > 1) {
        sprintf(tmp, "%s+%d %s", page_tag, num_unread - 1, age);
      } else {
        sprintf(tmp, "%s%s", page_tag, age);
      }
      int right_w = display.getTextWidth(tmp);
      display.setColor(UIColor::corp_blue);
      display.setCursor(display.width() - right_w - 1, 0);
      display.print(tmp);
      display.setColor(UIColor::secondary_txt);
      display.drawTextEllipsized(0, 0, display.width() - right_w - 4, filtered_origin);
      display.setColor(UIColor::corp_blue);
      display.drawRect(0, 11, display.width(), 1);  // horiz line
    } else {
      display.setCursor(0, 0);
      display.setColor(UIColor::corp_blue);
      sprintf(tmp, "%s: %d", replay ? _("Recent") : "Unread", num_unread);
      display.print(tmp);

      sprintf(tmp, "%s%s", page_tag, age);
      display.setCursor(display.width() - display.getTextWidth(tmp) - 2, 0);
      display.print(tmp);

      display.drawRect(0, 11, display.width(), 1);  // horiz line

      display.setColor(UIColor::secondary_txt);
      display.drawTextEllipsized(0, 14, display.width(), filtered_origin);
    }

    // A new message or text page replaces most of the ink. The partial wave
    // leaves the previous letters on the glass, so this frame is a full refresh.
    // The age counter alone stays partial.
    if (head != shown_head || page_start != shown_page) {
      display.markFull();
      shown_head = head;
      shown_page = page_start;
    }
#if AUTO_OFF_MILLIS==0 // probably e-ink
    if (paged) {
      long wait = (long)(page_until - millis());
      return wait > 200 ? wait : 200;
    }
    return 10000; // 10 s
#else
    return 1000;  // next render after 1000 ms
#endif
  }

  bool handleInput(char c) override {
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      bool was_group = unread[head].group;
      num_unread--;
      if (was_group) { if (groups > 0) groups--; }
      else if (chats > 0) chats--;
      if (num_unread == 0) {
        _task->gotoHomeScreen();
      } else {
        head = (head + 1) % MAX_UNREAD_MSGS;  // older message first, then newer
        resetPage();
      }
      return true;
    }
    if (c == KEY_SELECT) {  // triple click: drop this message from the buffer
      dropHead();
      return true;
    }
    if (c == KEY_PREV || c == KEY_DOWN) {   // manual page flip (double click on single-button boards)
      if (page_next > 0 || page_num > 0) nextPage();
      return true;
    }
    if (c == KEY_UP) {
      resetPage();
      return true;
    }
    if (c == KEY_ENTER) {
      num_unread = 0;  // clear unread queue
      groups = chats = 0;
      _task->gotoHomeScreen();
      return true;
    }
    return false;
  }
};

#endif

static void clockDraw(DisplayDriver& display, const ClockFace& face, int x, int y, const char* text) {
  for (const char* s = text; *s; ) {
    uint16_t cp;
    s = clockNext(s, cp);
    ClockGlyph g;
    if (!clockFind(face, cp, g)) continue;
    if (g.w && g.h) display.blit1(x, y + g.top, g.w, g.h, face.bits + g.offset);
    x += g.advance;
  }
}

class ClockScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;
  MsgPreviewScreen* _preview;

  NodePrefs* _prefs;

  static void formatTime(uint32_t t, int16_t tz_mins, char* buf) {
    if (t < 1000000000UL) {
      strcpy(buf, "--:--");
      return;
    }
    int64_t local = (int64_t)t + (int64_t)tz_mins * 60;
    if (local < 0) local = 0;
    uint32_t mins = ((uint32_t)local / 60UL) % (24UL * 60UL);
    sprintf(buf, "%02u:%02u", (unsigned)(mins / 60UL), (unsigned)(mins % 60UL));
  }

public:
  ClockScreen(UITask* task, mesh::RTCClock* rtc, MsgPreviewScreen* preview, NodePrefs* prefs)
    : _task(task), _rtc(rtc), _preview(preview), _prefs(prefs) {}

  int render(DisplayDriver& display) override {
    display.setColor(UIColor::primary_txt);
    int fw = display.frameWidth();
    int fh = display.frameHeight();
    const ClockFace& digits = (fw >= 220) ? CLOCK_DIGIT_WIDE : CLOCK_DIGIT_NARROW;

    char time_buf[8];
    uint32_t now = _rtc->getCurrentTime();
    int16_t tz = _prefs ? _prefs->tz_offset_mins : 0;
    formatTime(now, tz, time_buf);

    char date_buf[24] = "";
    int header_h = 0;
    if (now >= 1000000000UL) {
      formatClockDate(now, tz, _prefs ? _prefs->ui_lang : UI_LANG_RU, date_buf, sizeof(date_buf));
      int topd, botd;
      clockInk(CLOCK_HEADER, date_buf, topd, botd);
      int inkd = botd - topd;
      int y = 4 - topd;
      if (y < 1) y = 1;
      clockDraw(display, CLOCK_HEADER, (fw - clockWidth(CLOCK_HEADER, date_buf)) / 2, y, date_buf);
      header_h = inkd + 8;
    }

    char line1[24] = "";
    char line2[24] = "";
    int groups = _preview->groupCount();
    int chats = _preview->chatCount();
    bool en = _prefs && _prefs->ui_lang == UI_LANG_EN;
    if (groups > 0) sprintf(line1, en ? _("Groups: %d") : _("Groups: %d"), groups);
    if (chats > 0) sprintf(line2, en ? "Chat: %d" : _("Chats: %d"), chats);
    bool two = line1[0] && line2[0];
    char both[40];
    if (two) sprintf(both, "%s    %s", line1, line2);

    int footer_h = 0;
    if (line1[0] || line2[0]) {
      const char* shown = two ? both : (line1[0] ? line1 : line2);
      bool stack = two && clockWidth(CLOCK_FOOTER, both) > fw - 8;
      int top0, bot0, top1 = 0, bot1 = 0;
      clockInk(CLOCK_FOOTER, stack ? line1 : shown, top0, bot0);
      int ink = bot0 - top0;
      if (stack) {
        clockInk(CLOCK_FOOTER, line2, top1, bot1);
        ink += 4 + (bot1 - top1);
      }
      int y = fh - 6 - ink;
      if (!stack) {
        clockDraw(display, CLOCK_FOOTER, (fw - clockWidth(CLOCK_FOOTER, shown)) / 2, y - top0, shown);
      } else {
        clockDraw(display, CLOCK_FOOTER, (fw - clockWidth(CLOCK_FOOTER, line1)) / 2, y - top0, line1);
        int y2 = y + (bot0 - top0) + 4;
        clockDraw(display, CLOCK_FOOTER, (fw - clockWidth(CLOCK_FOOTER, line2)) / 2, y2 - top1, line2);
      }
      footer_h = ink + 12;
    }

    int top, bot;
    clockInk(digits, time_buf, top, bot);
    int ink = bot - top;
    int area = fh - footer_h - header_h;
    int y = header_h + (area - ink) / 2 - top;
    if (y < 2) y = 2;
    clockDraw(display, digits, (fw - clockWidth(digits, time_buf)) / 2, y, time_buf);

    if (now < 1000000000UL) return 60000;
    uint32_t sec = now % 60UL;
    int wait = (int)((60UL - sec) * 1000UL);
    if (wait < 1000) wait = 1000;
    return wait;
  }

  bool handleInput(char c) override {
    if (c == KEY_ENTER && _preview->reopen()) {
      _task->showMsgPreview();
      return true;
    }
    if (_preview->unreadCount() > 0) {
#ifdef HELTEC_LORA_V4
      _preview->reopen();
#endif
      _task->showMsgPreview();
    } else _task->gotoHomeScreen();
    return true;
  }
};

bool UITask::reopenMsgPreview() {
  if (!((MsgPreviewScreen *) msg_preview)->reopen()) return false;
  showMsgPreview();
  return true;
}

uint32_t UITask::autoOffMs() {
  // 0 = keep the screen on; the value comes from Настройки → Таймаут.
  static const uint16_t secs[5] = {0, 15, 30, 60, 120};
  uint8_t idx = _node_prefs ? _node_prefs->screen_off_idx : 3;
  if (idx >= 5) idx = 3;
  uint16_t s = secs[idx];
  return s ? (uint32_t)s * 1000UL : 0x7FFFFFFFUL;
}

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + autoOffMs();

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  _node_prefs = node_prefs;

  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
  buzzer.startup();
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
  msg_preview = new MsgPreviewScreen(this, &rtc_clock);
  clock = (display != NULL && display->isEink())
      ? new ClockScreen(this, &rtc_clock, (MsgPreviewScreen*)msg_preview, node_prefs) : NULL;
  _idle_since = millis();
  syncDisplayRefresh();
  setCurrScreen(splash);
}

void UITask::showAlert(const char* text, int duration_millis) {
  // A short plaque appears and then disappears, so e-ink pays for two full refreshes.
  if (_display && _display->isEink() && duration_millis <= 2000) return;
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::keepDisplayAwake() {
#if AUTO_OFF_MILLIS > 0
  _auto_off = millis() + autoOffMs();
#endif
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    // gemini's pick
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}

void UITask::refreshAfterIncomingMessage() {
#ifdef HELTEC_LORA_V4
  requestRefresh();
#else
  if (_display == NULL) return;
  if (!_display->isOn() && !hasConnection()) _display->turnOn();
  if (_display->isOn()) {
    _auto_off = millis() + autoOffMs();
    _next_refresh = 100;
  }
#endif
}

void UITask::onMessageRecv(mesh::Packet *pkt, const ContactInfo &from, uint8_t txt_type, uint32_t sender_timestamp, const char* text) {
  // we only want to show text messages on display, not cli data
  if (!(txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN)) return;

  uint8_t path_len = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  auto* preview = (MsgPreviewScreen*)msg_preview;
  preview->addPreview(path_len, from.name, text);
#ifndef HELTEC_LORA_V4
  setCurrScreen(msg_preview);
#endif

  refreshAfterIncomingMessage();

  if (!hasConnection()) {
    notify(UIEventType::contactMessage);
  }
}

int UITask::getDeviceStored() const {
  if (!msg_preview) return 0;
  return ((MsgPreviewScreen *) msg_preview)->storedCount();
}

int UITask::getUnreadCount() const {
  if (!msg_preview) return 0;
  return ((MsgPreviewScreen *) msg_preview)->unreadCount();
}

void UITask::clearTakenMsg() {
  if (!msg_preview) return;
#ifndef HELTEC_LORA_V4
  bool empty = ((MsgPreviewScreen *) msg_preview)->dropOldest();
  if (empty && curr == msg_preview) {
    gotoHomeScreen();
  }
#endif
  if (curr == msg_preview || curr == clock || curr == home) _next_refresh = 100;
}

void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
#ifdef HELTEC_LORA_V4
  if (curr == msg_preview || curr == clock || curr == home) _next_refresh = 100;
#else
  // The phone drains the offline queue as soon as a message arrives. That must
  // not dismiss the e-ink clock; the footer count already updated in newMsg.
  if (msgcount == 0 && curr != clock) {
    gotoHomeScreen();
  } else if (curr == home) {
    _next_refresh = 100;
  }
#endif
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount, bool group) {
  _msgcount = msgcount;

  auto* preview = (MsgPreviewScreen*)msg_preview;
  preview->addPreview(path_len, from_name, text, group);
#ifndef HELTEC_LORA_V4
  if (curr == clock) _next_refresh = 100;
  else setCurrScreen(msg_preview);
#endif

  refreshAfterIncomingMessage();

  if (!hasConnection()) {
    notify(UIEventType::contactMessage);
  }
}

void UITask::onChannelMessageRecv(mesh::Packet *pkt, ChannelDetails& channel_details, const char* text) {
  uint8_t path_len = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  auto* preview = (MsgPreviewScreen*)msg_preview;
#ifdef HELTEC_LORA_V4
  preview->addPreview(path_len, channel_details.name, text, true);
#else
  preview->addPreview(path_len, channel_details.name, text);
  setCurrScreen(msg_preview);
#endif

  refreshAfterIncomingMessage();

  if (!hasConnection()) {
    notify(UIEventType::channelMessage);
  }
}

void UITask::onQueueSizeChanged(int msgcount) {
#ifdef HELTEC_LORA_V4
  msgRead(msgcount);
#else
  _msgcount = msgcount;
  if (msgcount == 0) {
    gotoHomeScreen();
  }
#endif
}

void UITask::onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) {
  if (!hasConnection()) {
    notify(UIEventType::newContactMessage);
  }
}

void UITask::onControlDataRecv(const mesh::Packet* packet) {
#if UI_DISCOVER_SCREEN
  if (packet->payload_len >= 14 && (packet->payload[0] & 0xF0) == CTL_TYPE_NODE_DISCOVER_RESP) {
    ((HomeScreen *) home)->handleDiscoverResponse(packet);
  }
#endif
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
  if (c == home) _idle_since = millis();
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    display.forceFullRefresh();
    display.clear();
    display.endFrame();
    // Power off board including radio, display, GPS and components
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

void UITask::loop() {
  char c = 0;
#if UI_HAS_JOYSTICK
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);  // REVISIT: could be mapped to different key code
  }
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_LEFT);
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_RIGHT);
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  #ifdef UI_HAS_NAV_INPUT
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    display.turnOff();
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_SELECT);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
  #else
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  } else if (ev == BUTTON_EVENT_QUAD_CLICK) {
    c = handleQuadClick(KEY_QUAD);
  }
  #endif  
#endif
#if defined(UI_HAS_ROTARY_INPUT)
  RotaryInputEvent rotaryEv = rotary_input.poll();
  if (c == 0 && _display != NULL && _display->isOn()) {
    if (rotaryEv == RotaryInputEvent::Next) {
      c = KEY_NEXT;
    } else if (rotaryEv == RotaryInputEvent::Prev) {
      c = KEY_PREV;
    }
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    int ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    } else if (ev == BUTTON_EVENT_QUAD_CLICK) {
      c = handleQuadClick(KEY_QUAD);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
    _idle_since = millis();
    _auto_off = millis() + autoOffMs();   // extend auto-off timer
    _next_refresh = 100;  // trigger refresh
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry) {  // render alert popup
        _display->setTextSize(1);
        int y = _display->height() / 3;
        int p = _display->height() / 32;
        _display->setColor(UIColor::popup_bkg);
        _display->fillRect(p, y, _display->width() - p*2, y);
        _display->setColor(UIColor::popup_txt);  // draw box border
        _display->drawRect(p, y, _display->width() - p*2, y);
#ifdef HELTEC_LORA_V4
        const int inset = p + OLED_INSET;
        if (_display->getTextWidth(_alert) > _display->width() - 2 * inset) {
          _display->drawTextEllipsized(inset, y + p * 3, _display->width() - 2 * inset, _alert);
        } else
#endif
        {
          _display->drawTextCentered(_display->width() / 2, y + p * 3, _alert);
        }
        _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
      } else {
        _next_refresh = millis() + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
#ifdef KEEP_DISPLAY_ON_USB
    // Opt-in: refresh the auto-off deadline while externally powered, so the
    // timer counts from the moment external power is removed. Off by default
    // because OLED panels burn in quickly; only enable for LCD targets or
    // where the display is replaceable.
    if (board.isExternalPowered()) {
      _auto_off = millis() + autoOffMs();
    }
#endif
    if (millis() > _auto_off) {
      _display->turnOff();
    }
#endif
  }

  if (_display != NULL && _display->isEink() && clock != NULL && curr == home) {
    if (((HomeScreen*)home)->allowsClock() && (long)(millis() - _idle_since) >= 60000L) {
      setCurrScreen(clock);
    }
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {
      if(!board.isExternalPowered()) {
        if (_display != NULL) {
          _display->startFrame();
          int text_size = 2;
#ifdef HELTEC_LORA_V4
          if (_display->width() == 128) text_size = 1;
#endif
          _display->setTextSize(text_size);
          _display->setColor(UIColor::warning_txt);
          _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
          _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
          _display->endFrame();
          if (_display->isEink() == false) { delay(3000); }
        }
        shutdown();
      }
    }
    next_batt_chck = millis() + 8000;
  }
#endif
}

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();   // turn display on and consume event
      c = 0;
    }
    _auto_off = millis() + autoOffMs();   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c) {
  c = checkDisplayOn(c);
  if (c == 0) return 0;
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    c = 0;   // consume event
  }
  if (c == 0) return 0;
  // Long press on Bluetooth opens Connections. Inside, the selected row
  // receives KEY_ENTER and can be toggled explicitly.
  if (curr == home) {
    HomeScreen* hs = static_cast<HomeScreen*>(curr);
    if (hs->getPage() == HomeScreen::HomePage::BLUETOOTH) {
      // Long press on Bluetooth = enter the CONNECTIONS sub-screen
      hs->enterSub(HomeScreen::HomePage::CONNECTIONS);
      c = 0;
    }
  }
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  return checkDisplayOn(c);
}

char UITask::handleQuadClick(char c) {
  c = checkDisplayOn(c);
  if (c == 0) return 0;
#ifdef HELTEC_LORA_V4
  static_cast<HomeScreen*>(home)->returnToMain();
  _alert_expiry = 0;
  gotoHomeScreen();
  requestRefresh();
  return 0;
#else
  if (curr == home) return c;
  return 0;
#endif
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  c = checkDisplayOn(c);
  if (c == 0) return 0;
  if (curr == home || curr == msg_preview) return c;
  toggleBuzzer();
  return 0;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  }
  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          _node_prefs->gps_enabled = 0;
          notify(UIEventType::ack);
        } else {
          _sensors->setSettingValue("gps", "1");
          _node_prefs->gps_enabled = 1;
          notify(UIEventType::ack);
        }
        the_mesh.savePrefs();
        if (_node_prefs->gps_enabled && _node_prefs->mcu_sleep) {
          showAlert("GPS needs CPU on", 1200);
        } else {
          showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
        }
        _next_refresh = 0;
        break;
      }
    }
  }
}

bool UITask::isDisplayOn() const {
#if AUTO_OFF_MILLIS > 0
  return _display != NULL && _display->isOn();
#else
  return false;
#endif
}

void UITask::applyPowerProfile(bool rx_boost, bool fem_lna, bool cpu_sleep) {
  if (!radio_driver.setRxBoostedGainMode(rx_boost)) {
    showAlert("RX boost failed", 900);
    return;
  }
  _node_prefs->rx_boosted_gain = rx_boost ? 1 : 0;
  bool cpu_blocked = false;
  bool ble_blocked = bleBlocksCpuSleep();
  if (cpu_sleep && (gpsBlocksCpuSleep() || ble_blocked)) {
    cpu_sleep = false;
    cpu_blocked = true;
  }
  if (_board && _board->canControlLoRaFemLna()) {
    if (!_board->setLoRaFemLnaEnabled(fem_lna)) {
      the_mesh.savePrefs();
      showAlert("LNA failed", 900);
      _next_refresh = 0;
      return;
    }
    _node_prefs->radio_fem_rxgain = fem_lna ? 1 : 0;
  }
  _node_prefs->mcu_sleep = (cpu_sleep && canSelectMcuSleep()) ? 1 : 0;
  the_mesh.savePrefs();
  notify(UIEventType::ack);
  bool fem = canControlFemLna();
  bool cpu_ok = canSelectMcuSleep();
  PowerChoice choices[6];
  int n = buildPowerChoices(fem, cpu_ok, choices);
  bool boost = _node_prefs->rx_boosted_gain != 0;
  bool lna = fem && _node_prefs->radio_fem_rxgain != 0;
  bool cpu = _node_prefs->mcu_sleep != 0;
  const char* name = "Custom";
  for (int i = 0; i < n; i++) {
    if (choices[i].boost == boost && choices[i].lna == lna && choices[i].cpu == cpu) {
      name = choices[i].name;
      break;
    }
  }
  if (cpu_blocked) showAlert(ble_blocked ? "BLE needs CPU on" : "GPS needs CPU on", 1200);
  else showAlert(name, 800);
  _next_refresh = 0;
}

void UITask::cycleRadioProfile() {
  uint8_t next = radioNextProfile(the_mesh.radioProfile());
  the_mesh.applyRadioProfile(next);
  notify(UIEventType::ack);
  showAlert(radioProfileAlert(next), 900);
  _next_refresh = 0;
}

void UITask::toggleFemLna() {
  if (!_board || !_board->canControlLoRaFemLna()) {
    showAlert("LNA: n/a", 800);
    return;
  }
  bool enable = !_board->isLoRaFemLnaEnabled();
  if (!_board->setLoRaFemLnaEnabled(enable)) {
    showAlert("LNA: failed", 800);
    return;
  }
  _node_prefs->radio_fem_rxgain = enable ? 1 : 0;
  the_mesh.savePrefs();
  notify(UIEventType::ack);
  showAlert(enable ? _("LNA: on") : _("LNA: off"), 800);
  _next_refresh = 0;
}

void UITask::toggleBuzzer() {
    // Toggle buzzer quiet mode
  #ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
    } else {
      buzzer.quiet(true);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    showAlert(buzzer.isQuiet() ? "Buzzer: OFF" : "Buzzer: ON", 800);
    _next_refresh = 0;  // trigger refresh
  #endif
}
