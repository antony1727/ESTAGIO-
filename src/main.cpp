#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include "LGFX_ESP32_8048S070.h"
#include "app_config.h"
#include "web_server.h"
#include "ota_updater.h"
#include "version.h"

LGFX tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;
#define BUF_LINES 32 // 800*32 = 25600 px ~50KB

// UI Widgets - Top Status Bar
static lv_obj_t *top_wifi_label = nullptr;
static lv_obj_t *top_uptime_label = nullptr;
static lv_obj_t *top_ram_label = nullptr;
static lv_obj_t *top_refresh_btn = nullptr;

// UI Widgets - Left Panel
static lv_obj_t *time_label = nullptr;
static lv_obj_t *date_label = nullptr;
static lv_obj_t *weather_city_label = nullptr;
static lv_obj_t *weather_humidity_label = nullptr;
static lv_obj_t *weather_wind_label = nullptr;
static lv_obj_t *weather_icon_box = nullptr;

// UI Widgets - Weekly Forecast (5 Days)
static lv_obj_t *forecast_day_labels[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *forecast_icon_boxes[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *forecast_temp_labels[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

// UI Widgets - Right Panel (Moedas)
static lv_obj_t *moeda_pair_labels[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *moeda_sub_labels[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *moeda_value_labels[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *moeda_pct_labels[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *moeda_icon_boxes[3] = {nullptr, nullptr, nullptr};

// Variáveis de Estado
static String moedaValues[6] = {"R$ --,--", "R$ --,--", "R$ --,--", "R$ --,--", "R$ --,--", "R$ --,--"};
static String moedaPcts[6] = {"--", "--", "--", "--", "--", "--"};
static bool moedaPctPos[6] = {true, true, true, true, true, true};
String dolarValue = "R$ --,--";
String weatherTemp = "--";
String weatherDesc = "----";
String weatherCity = "Nepomuceno";
String weatherHumidity = "Umidade: --%";
String weatherWind = "Vento: -- km/h";
int currentWeatherCode = 0;

// Dados da Previsão Semanal (5 Dias)
struct DayForecast {
  String dayName;
  int tempMax;
  int weatherCode;
};
static DayForecast weeklyForecast[5] = {
  {"QUA", 25, 1},
  {"QUI", 25, 2},
  {"SEX", 25, 2},
  {"SAB", 23, 61},
  {"DOM", 15, 61}
};

volatile bool gNeedsRebuild = false;

// Remove acentos para compatibilidade total com fontes ASCII do LVGL
String sanitize_for_lvgl(String str) {
  String s = str;
  s.replace("á", "a"); s.replace("à", "a"); s.replace("ã", "a"); s.replace("â", "a"); s.replace("ä", "a");
  s.replace("Á", "A"); s.replace("À", "A"); s.replace("Ã", "A"); s.replace("Â", "A"); s.replace("Ä", "A");
  s.replace("é", "e"); s.replace("ê", "e"); s.replace("è", "e"); s.replace("ë", "e");
  s.replace("É", "E"); s.replace("Ê", "E"); s.replace("È", "E"); s.replace("Ë", "E");
  s.replace("í", "i"); s.replace("ì", "i"); s.replace("î", "i"); s.replace("ï", "i");
  s.replace("Í", "I"); s.replace("Ì", "I"); s.replace("Î", "I"); s.replace("Ï", "I");
  s.replace("ó", "o"); s.replace("õ", "o"); s.replace("ô", "o"); s.replace("ò", "o"); s.replace("ö", "o");
  s.replace("Ó", "O"); s.replace("Õ", "O"); s.replace("Ô", "O"); s.replace("Ò", "O"); s.replace("Ö", "O");
  s.replace("ú", "u"); s.replace("ù", "u"); s.replace("û", "u"); s.replace("ü", "u");
  s.replace("Ú", "U"); s.replace("Ù", "U"); s.replace("Û", "U"); s.replace("Ü", "U");
  s.replace("ç", "c"); s.replace("Ç", "C");
  s.replace("º", "");  s.replace("ª", "");
  return s;
}

// Uptime formatado em português
String get_uptime_str() {
  unsigned long s = millis() / 1000;
  unsigned long d = s / 86400;
  unsigned long h = (s % 86400) / 3600;
  unsigned long m = (s % 3600) / 60;
  char buf[32];
  if (d > 0) {
    snprintf(buf, sizeof(buf), "Uptime: %lu dias %luh", d, h);
  } else if (h > 0) {
    snprintf(buf, sizeof(buf), "Uptime: %luh %lum", h, m);
  } else {
    snprintf(buf, sizeof(buf), "Uptime: %lum", m);
  }
  return String(buf);
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

void my_touch_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Nomes amigáveis das moedas
const char* get_currency_friendly_name(const char* pair) {
  if (strstr(pair, "BTC")) return "Bitcoin";
  if (strstr(pair, "USD")) return "Dolar";
  if (strstr(pair, "EUR")) return "Euro";
  if (strstr(pair, "ETH")) return "Ethereum";
  if (strstr(pair, "USDT")) return "Tether";
  if (strstr(pair, "GBP")) return "Libra";
  if (strstr(pair, "JPY")) return "Iene";
  if (strstr(pair, "CAD")) return "Dolar Can.";
  if (strstr(pair, "CHF")) return "Franco Suico";
  if (strstr(pair, "ARS")) return "Peso Arg.";
  if (strstr(pair, "SOL")) return "Solana";
  return "Cambio";
}

// Renderizador dos ícones de Moedas / Bandeiras
void render_currency_icon(lv_obj_t *parent, const char* pair) {
  lv_obj_clean(parent);

  if (strstr(pair, "BTC")) {
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_set_size(circle, 40, 40);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0xF7931A), 0);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(circle, 0, 0);
    lv_obj_set_style_pad_all(circle, 0, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sym = lv_label_create(circle);
    lv_label_set_text(sym, "B");
    lv_obj_set_style_text_font(sym, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sym, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(sym, LV_ALIGN_CENTER, 0, 0);
  }
  else if (strstr(pair, "USD") && !strstr(pair, "USDT")) {
    lv_obj_t *flag = lv_obj_create(parent);
    lv_obj_set_size(flag, 42, 28);
    lv_obj_align(flag, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(flag, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(flag, 4, 0);
    lv_obj_set_style_border_width(flag, 1, 0);
    lv_obj_set_style_border_color(flag, lv_color_hex(0x475569), 0);
    lv_obj_set_style_pad_all(flag, 0, 0);
    lv_obj_clear_flag(flag, LV_OBJ_FLAG_SCROLLABLE);

    for (int s = 0; s < 3; s++) {
      lv_obj_t *stripe = lv_obj_create(flag);
      lv_obj_set_size(stripe, 42, 4);
      lv_obj_set_pos(stripe, 0, 4 + s * 8);
      lv_obj_set_style_bg_color(stripe, lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_radius(stripe, 0, 0);
      lv_obj_set_style_border_width(stripe, 0, 0);
    }

    lv_obj_t *canton = lv_obj_create(flag);
    lv_obj_set_size(canton, 20, 15);
    lv_obj_set_pos(canton, 0, 0);
    lv_obj_set_style_bg_color(canton, lv_color_hex(0x1E3A8A), 0);
    lv_obj_set_style_radius(canton, 0, 0);
    lv_obj_set_style_border_width(canton, 0, 0);

    lv_obj_t *star = lv_obj_create(canton);
    lv_obj_set_size(star, 4, 4);
    lv_obj_align(star, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(star, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(star, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(star, 0, 0);
  }
  else if (strstr(pair, "ETH")) {
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_set_size(circle, 40, 40);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(circle, 0, 0);
    lv_obj_set_style_pad_all(circle, 0, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sym = lv_label_create(circle);
    lv_label_set_text(sym, "ETH");
    lv_obj_set_style_text_font(sym, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sym, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(sym, LV_ALIGN_CENTER, 0, 0);
  }
  else if (strstr(pair, "EUR")) {
    lv_obj_t *flag = lv_obj_create(parent);
    lv_obj_set_size(flag, 42, 28);
    lv_obj_align(flag, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(flag, lv_color_hex(0x003399), 0);
    lv_obj_set_style_radius(flag, 4, 0);
    lv_obj_set_style_border_width(flag, 1, 0);
    lv_obj_set_style_border_color(flag, lv_color_hex(0x475569), 0);
    lv_obj_set_style_pad_all(flag, 0, 0);
    lv_obj_clear_flag(flag, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ring = lv_obj_create(flag);
    lv_obj_set_size(ring, 16, 16);
    lv_obj_align(ring, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0xFBBF24), 0);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  }
  else {
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_set_size(circle, 40, 40);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0x6366F1), 0);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(circle, 0, 0);
    lv_obj_set_style_pad_all(circle, 0, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    char code[4] = {0};
    strncpy(code, pair, 3);
    lv_obj_t *sym = lv_label_create(circle);
    lv_label_set_text(sym, code);
    lv_obj_set_style_text_font(sym, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sym, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(sym, LV_ALIGN_CENTER, 0, 0);
  }
}

// Renderizador do Ícone Grande de Clima (Sol atrás de nuvem + chuva)
void render_weather_icon(lv_obj_t *parent, int wcode) {
  if (!parent) return;
  lv_obj_clean(parent);

  bool isSunnyOnly = (wcode == 0);
  bool isCloudyOnly = (wcode == 3 || wcode == 45 || wcode == 48);
  bool isRain = (wcode >= 51 && wcode <= 67) || (wcode >= 80 && wcode <= 82);
  bool isThunder = (wcode >= 95);

  // Sol
  if (!isCloudyOnly && !isThunder) {
    lv_obj_t *sun = lv_obj_create(parent);
    int sunSize = isSunnyOnly ? 44 : 32;
    lv_obj_set_size(sun, sunSize, sunSize);
    lv_obj_set_pos(sun, isSunnyOnly ? 12 : 26, isSunnyOnly ? 6 : 2);
    lv_obj_set_style_bg_color(sun, lv_color_hex(0xFBBF24), 0);
    lv_obj_set_style_radius(sun, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sun, 0, 0);
    lv_obj_set_style_shadow_width(sun, 14, 0);
    lv_obj_set_style_shadow_color(sun, lv_color_hex(0xF59E0B), 0);
    lv_obj_set_style_shadow_opa(sun, 180, 0);
    lv_obj_clear_flag(sun, LV_OBJ_FLAG_SCROLLABLE);

    for (int r = 0; r < 4; r++) {
      lv_obj_t *ray = lv_obj_create(parent);
      lv_obj_set_size(ray, 4, 4);
      if (r == 0) lv_obj_set_pos(ray, isSunnyOnly ? 32 : 40, isSunnyOnly ? 0 : 0);
      else if (r == 1) lv_obj_set_pos(ray, isSunnyOnly ? 58 : 60, isSunnyOnly ? 26 : 16);
      else if (r == 2) lv_obj_set_pos(ray, isSunnyOnly ? 32 : 40, isSunnyOnly ? 52 : 36);
      else if (r == 3) lv_obj_set_pos(ray, isSunnyOnly ? 6 : 20, isSunnyOnly ? 26 : 16);
      lv_obj_set_style_bg_color(ray, lv_color_hex(0xFBBF24), 0);
      lv_obj_set_style_radius(ray, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_border_width(ray, 0, 0);
      lv_obj_clear_flag(ray, LV_OBJ_FLAG_SCROLLABLE);
    }
  }

  // Nuvem
  if (!isSunnyOnly) {
    lv_color_t cloudCol = (isThunder || (wcode >= 61 && wcode <= 67)) ? lv_color_hex(0x94A3B8) : lv_color_hex(0xE2E8F0);

    lv_obj_t *cBase = lv_obj_create(parent);
    lv_obj_set_size(cBase, 48, 22);
    lv_obj_set_pos(cBase, 2, 24);
    lv_obj_set_style_bg_color(cBase, cloudCol, 0);
    lv_obj_set_style_radius(cBase, 11, 0);
    lv_obj_set_style_border_width(cBase, 0, 0);
    lv_obj_set_style_shadow_width(cBase, 8, 0);
    lv_obj_set_style_shadow_color(cBase, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(cBase, 40, 0);
    lv_obj_clear_flag(cBase, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cDome1 = lv_obj_create(parent);
    lv_obj_set_size(cDome1, 24, 24);
    lv_obj_set_pos(cDome1, 10, 12);
    lv_obj_set_style_bg_color(cDome1, cloudCol, 0);
    lv_obj_set_style_radius(cDome1, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(cDome1, 0, 0);
    lv_obj_clear_flag(cDome1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cDome2 = lv_obj_create(parent);
    lv_obj_set_size(cDome2, 18, 18);
    lv_obj_set_pos(cDome2, 28, 16);
    lv_obj_set_style_bg_color(cDome2, cloudCol, 0);
    lv_obj_set_style_radius(cDome2, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(cDome2, 0, 0);
    lv_obj_clear_flag(cDome2, LV_OBJ_FLAG_SCROLLABLE);
  }

  // Chuva
  if (isRain) {
    for (int d = 0; d < 3; d++) {
      lv_obj_t *drop = lv_obj_create(parent);
      lv_obj_set_size(drop, 3, 8);
      lv_obj_set_pos(drop, 12 + d * 12, 48);
      lv_obj_set_style_bg_color(drop, lv_color_hex(0x38BDF8), 0);
      lv_obj_set_style_radius(drop, 2, 0);
      lv_obj_set_style_border_width(drop, 0, 0);
      lv_obj_clear_flag(drop, LV_OBJ_FLAG_SCROLLABLE);
    }
  } else if (isThunder) {
    lv_obj_t *bolt = lv_obj_create(parent);
    lv_obj_set_size(bolt, 6, 12);
    lv_obj_set_pos(bolt, 22, 46);
    lv_obj_set_style_bg_color(bolt, lv_color_hex(0xFACC15), 0);
    lv_obj_set_style_radius(bolt, 2, 0);
    lv_obj_set_style_border_width(bolt, 0, 0);
    lv_obj_clear_flag(bolt, LV_OBJ_FLAG_SCROLLABLE);
  }
}

// Mini Ícones de Clima para a Previsão Semanal
void render_mini_weather_icon(lv_obj_t *parent, int wcode) {
  if (!parent) return;
  lv_obj_clean(parent);

  bool isSunnyOnly = (wcode == 0);
  bool isCloudyOnly = (wcode == 3 || wcode == 45 || wcode == 48);
  bool isRain = (wcode >= 51 && wcode <= 67) || (wcode >= 80 && wcode <= 82);

  if (!isCloudyOnly) {
    lv_obj_t *sun = lv_obj_create(parent);
    lv_obj_set_size(sun, 14, 14);
    lv_obj_set_pos(sun, isSunnyOnly ? 9 : 14, 2);
    lv_obj_set_style_bg_color(sun, lv_color_hex(0xFBBF24), 0);
    lv_obj_set_style_radius(sun, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sun, 0, 0);
    lv_obj_clear_flag(sun, LV_OBJ_FLAG_SCROLLABLE);
  }
  if (!isSunnyOnly) {
    lv_color_t cloudCol = isRain ? lv_color_hex(0x94A3B8) : lv_color_hex(0xE2E8F0);
    lv_obj_t *cloud = lv_obj_create(parent);
    lv_obj_set_size(cloud, 24, 12);
    lv_obj_set_pos(cloud, 4, 10);
    lv_obj_set_style_bg_color(cloud, cloudCol, 0);
    lv_obj_set_style_radius(cloud, 6, 0);
    lv_obj_set_style_border_width(cloud, 0, 0);
    lv_obj_clear_flag(cloud, LV_OBJ_FLAG_SCROLLABLE);
  }
  if (isRain) {
    for (int d = 0; d < 2; d++) {
      lv_obj_t *drop = lv_obj_create(parent);
      lv_obj_set_size(drop, 2, 5);
      lv_obj_set_pos(drop, 10 + d * 6, 23);
      lv_obj_set_style_bg_color(drop, lv_color_hex(0x38BDF8), 0);
      lv_obj_set_style_radius(drop, 1, 0);
      lv_obj_set_style_border_width(drop, 0, 0);
      lv_obj_clear_flag(drop, LV_OBJ_FLAG_SCROLLABLE);
    }
  }
}

// Handler do Botão Atualizar
static void refresh_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    Serial.println("[Touch] Botão Atualizar pressionado!");
    extern void update_dolar(lv_timer_t *timer);
    extern void update_weather(lv_timer_t *timer);
    update_dolar(NULL);
    update_weather(NULL);
  }
}

// Criação da Interface Principal Fiel à Foto de Referência
void create_ui() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);

  bool isLight = gConfig.display_light;

  lv_color_t colBg = isLight ? lv_color_hex(0xF1F5F9) : lv_color_hex(0x070B14);
  lv_color_t colCard = isLight ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x0D192E);
  lv_color_t colSubCard = isLight ? lv_color_hex(0xF8FAFC) : lv_color_hex(0x081020);
  lv_color_t colBorder = isLight ? lv_color_hex(0xCBD5E1) : lv_color_hex(0x1E2E48);
  lv_color_t colTopBar = isLight ? lv_color_hex(0xE2E8F0) : lv_color_hex(0x0B111E);
  lv_color_t colGold = lv_color_hex(0xF59E0B);
  lv_color_t colWhite = isLight ? lv_color_hex(0x0F172A) : lv_color_hex(0xFFFFFF);
  lv_color_t colMuted = isLight ? lv_color_hex(0x64748B) : lv_color_hex(0x94A3B8);

  lv_obj_set_style_bg_color(scr, colBg, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // ==========================================
  // BARRA SUPERIOR DE STATUS (800 x 38)
  // ==========================================
  lv_obj_t *top_bar = lv_obj_create(scr);
  lv_obj_set_pos(top_bar, 0, 0);
  lv_obj_set_size(top_bar, 800, 38);
  lv_obj_set_style_bg_color(top_bar, colTopBar, 0);
  lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(top_bar, 0, 0);
  lv_obj_set_style_border_width(top_bar, 1, 0);
  lv_obj_set_style_border_color(top_bar, colBorder, 0);
  lv_obj_set_style_pad_all(top_bar, 0, 0);
  lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

  // 1. Wi-Fi & IP
  top_wifi_label = lv_label_create(top_bar);
  if (WiFi.status() == WL_CONNECTED) {
    char buf[64];
    snprintf(buf, sizeof(buf), "WiFi: %s (%ddBm)", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
    lv_label_set_text(top_wifi_label, buf);
  } else {
    lv_label_set_text(top_wifi_label, "WiFi: Modo AP (192.168.4.1)");
  }
  lv_obj_set_style_text_font(top_wifi_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(top_wifi_label, colWhite, 0);
  lv_obj_set_pos(top_wifi_label, 16, 11);

  // 2. Uptime
  top_uptime_label = lv_label_create(top_bar);
  lv_label_set_text(top_uptime_label, get_uptime_str().c_str());
  lv_obj_set_style_text_font(top_uptime_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(top_uptime_label, colWhite, 0);
  lv_obj_set_pos(top_uptime_label, 270, 11);

  // 3. RAM Livre
  top_ram_label = lv_label_create(top_bar);
  char ramBuf[32];
  snprintf(ramBuf, sizeof(ramBuf), "RAM: %d KB Livre", (int)(ESP.getFreeHeap() / 1024));
  lv_label_set_text(top_ram_label, ramBuf);
  lv_obj_set_style_text_font(top_ram_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(top_ram_label, colWhite, 0);
  lv_obj_set_pos(top_ram_label, 440, 11);

  // 4. Avatar do Usuário
  lv_obj_t *avatar = lv_obj_create(top_bar);
  lv_obj_set_size(avatar, 24, 24);
  lv_obj_set_pos(avatar, 640, 7);
  lv_obj_set_style_bg_color(avatar, lv_color_hex(0x60A5FA), 0);
  lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(avatar, 0, 0);
  lv_obj_set_style_pad_all(avatar, 0, 0);
  lv_obj_clear_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *avSym = lv_label_create(avatar);
  lv_label_set_text(avSym, "U");
  lv_obj_set_style_text_font(avSym, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(avSym, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(avSym, LV_ALIGN_CENTER, 0, 0);

  // 5. Botão Atualizar (Clicável)
  top_refresh_btn = lv_btn_create(top_bar);
  lv_obj_set_size(top_refresh_btn, 110, 26);
  lv_obj_set_pos(top_refresh_btn, 674, 6);
  lv_obj_set_style_bg_color(top_refresh_btn, lv_color_hex(0x1E293B), 0);
  lv_obj_set_style_radius(top_refresh_btn, 13, 0);
  lv_obj_set_style_border_width(top_refresh_btn, 1, 0);
  lv_obj_set_style_border_color(top_refresh_btn, lv_color_hex(0x3B82F6), 0);
  lv_obj_add_event_cb(top_refresh_btn, refresh_btn_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnLbl = lv_label_create(top_refresh_btn);
  lv_label_set_text(btnLbl, "Atualizar");
  lv_obj_set_style_text_font(btnLbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(btnLbl, lv_color_hex(0x60A5FA), 0);
  lv_obj_align(btnLbl, LV_ALIGN_CENTER, 0, 0);

  // ==========================================
  // PAINEL ESQUERDO: DATA + HORA + CLIMA
  // ==========================================
  // 1. Data (ex: "QUA, 26 AGO")
  date_label = lv_label_create(scr);
  lv_label_set_text(date_label, "QUA, 26 AGO");
  lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(date_label, colMuted, 0);
  lv_obj_set_style_text_letter_space(date_label, 1, 0);
  lv_obj_align(date_label, LV_ALIGN_TOP_LEFT, 130, 46);

  // 2. Relógio Digital Grande (ex: "11:49")
  time_label = lv_label_create(scr);
  lv_label_set_text(time_label, "--:--");
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(time_label, colWhite, 0);
  lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 130, 70);

  // 3. Card Principal do Clima (368 x 336)
  lv_obj_t *weather_card = lv_obj_create(scr);
  lv_obj_set_pos(weather_card, 16, 132);
  lv_obj_set_size(weather_card, 368, 336);
  lv_obj_set_style_bg_color(weather_card, colCard, 0);
  lv_obj_set_style_bg_opa(weather_card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(weather_card, 14, 0);
  lv_obj_set_style_border_width(weather_card, 1, 0);
  lv_obj_set_style_border_color(weather_card, colBorder, 0);
  lv_obj_set_style_pad_all(weather_card, 10, 0);
  lv_obj_clear_flag(weather_card, LV_OBJ_FLAG_SCROLLABLE);

  // Cidade (ex: "NEPOMUCENO")
  weather_city_label = lv_label_create(weather_card);
  String cUpper = sanitize_for_lvgl(weatherCity);
  cUpper.toUpperCase();
  lv_label_set_text(weather_city_label, cUpper.c_str());
  lv_obj_set_style_text_font(weather_city_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(weather_city_label, colWhite, 0);
  lv_obj_set_style_text_letter_space(weather_city_label, 1, 0);
  lv_obj_align(weather_city_label, LV_ALIGN_TOP_MID, 0, 4);

  // Ilustração do Clima Atual
  weather_icon_box = lv_obj_create(weather_card);
  lv_obj_set_size(weather_icon_box, 70, 58);
  lv_obj_set_pos(weather_icon_box, 20, 38);
  lv_obj_set_style_bg_opa(weather_icon_box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(weather_icon_box, 0, 0);
  lv_obj_set_style_pad_all(weather_icon_box, 0, 0);
  lv_obj_clear_flag(weather_icon_box, LV_OBJ_FLAG_SCROLLABLE);
  render_weather_icon(weather_icon_box, currentWeatherCode);

  // Umidade (em português correto)
  weather_humidity_label = lv_label_create(weather_card);
  lv_label_set_text(weather_humidity_label, weatherHumidity.c_str());
  lv_obj_set_style_text_font(weather_humidity_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(weather_humidity_label, colWhite, 0);
  lv_obj_set_pos(weather_humidity_label, 110, 42);

  // Vento (em português correto)
  weather_wind_label = lv_label_create(weather_card);
  lv_label_set_text(weather_wind_label, weatherWind.c_str());
  lv_obj_set_style_text_font(weather_wind_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(weather_wind_label, colWhite, 0);
  lv_obj_set_pos(weather_wind_label, 110, 70);

  // Sub-card: Previsão Semanal (5 Dias)
  lv_obj_t *forecast_card = lv_obj_create(weather_card);
  lv_obj_set_pos(forecast_card, 6, 120);
  lv_obj_set_size(forecast_card, 336, 186);
  lv_obj_set_style_bg_color(forecast_card, colSubCard, 0);
  lv_obj_set_style_bg_opa(forecast_card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(forecast_card, 10, 0);
  lv_obj_set_style_border_width(forecast_card, 1, 0);
  lv_obj_set_style_border_color(forecast_card, colBorder, 0);
  lv_obj_set_style_pad_all(forecast_card, 6, 0);
  lv_obj_clear_flag(forecast_card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *fcTitle = lv_label_create(forecast_card);
  lv_label_set_text(fcTitle, "Previsao Semanal");
  lv_obj_set_style_text_font(fcTitle, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(fcTitle, colMuted, 0);
  lv_obj_set_pos(fcTitle, 8, 6);

  int colX[5] = {6, 72, 138, 204, 270};
  for (int i = 0; i < 5; i++) {
    // Dia da semana
    lv_obj_t *dLbl = lv_label_create(forecast_card);
    lv_label_set_text(dLbl, weeklyForecast[i].dayName.c_str());
    lv_obj_set_style_text_font(dLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(dLbl, colWhite, 0);
    lv_obj_set_pos(dLbl, colX[i] + 12, 38);
    forecast_day_labels[i] = dLbl;

    // Mini Ícone
    lv_obj_t *iconCont = lv_obj_create(forecast_card);
    lv_obj_set_size(iconCont, 32, 32);
    lv_obj_set_pos(iconCont, colX[i] + 10, 68);
    lv_obj_set_style_bg_opa(iconCont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(iconCont, 0, 0);
    lv_obj_set_style_pad_all(iconCont, 0, 0);
    lv_obj_clear_flag(iconCont, LV_OBJ_FLAG_SCROLLABLE);
    render_mini_weather_icon(iconCont, weeklyForecast[i].weatherCode);
    forecast_icon_boxes[i] = iconCont;

    // Temperatura Máxima
    char tBuf[16];
    snprintf(tBuf, sizeof(tBuf), "%d C", weeklyForecast[i].tempMax);
    lv_obj_t *tLbl = lv_label_create(forecast_card);
    lv_label_set_text(tLbl, tBuf);
    lv_obj_set_style_text_font(tLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(tLbl, colWhite, 0);
    lv_obj_set_pos(tLbl, colX[i] + 10, 126);
    forecast_temp_labels[i] = tLbl;
  }

  // ==========================================
  // PAINEL DIREITO: COTAÇÃO DE MOEDAS UNIFICADO
  // ==========================================
  lv_obj_t *moeda_title = lv_label_create(scr);
  lv_label_set_text(moeda_title, "COTACAO DE MOEDAS");
  lv_obj_set_style_text_font(moeda_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(moeda_title, colGold, 0);
  lv_obj_set_style_text_letter_space(moeda_title, 2, 0);
  lv_obj_align(moeda_title, LV_ALIGN_TOP_MID, 195, 46);

  lv_obj_t *moeda_container = lv_obj_create(scr);
  lv_obj_set_pos(moeda_container, 400, 80);
  lv_obj_set_size(moeda_container, 384, 388);
  lv_obj_set_style_bg_color(moeda_container, colCard, 0);
  lv_obj_set_style_bg_opa(moeda_container, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(moeda_container, 14, 0);
  lv_obj_set_style_border_width(moeda_container, 1, 0);
  lv_obj_set_style_border_color(moeda_container, colBorder, 0);
  lv_obj_set_style_pad_all(moeda_container, 0, 0);
  lv_obj_clear_flag(moeda_container, LV_OBJ_FLAG_SCROLLABLE);

  const char* defaultPairs[3] = {"BTC-BRL", "USD-BRL", "ETH-BRL"};
  const char* pairs[6] = {gConfig.currency_1, gConfig.currency_2, gConfig.currency_3, gConfig.currency_4, gConfig.currency_5, gConfig.currency_6};
  bool enabled[6] = {gConfig.curr1_enabled, gConfig.curr2_enabled, gConfig.curr3_enabled, gConfig.curr4_enabled, gConfig.curr5_enabled, gConfig.curr6_enabled};

  int activeIdx[3] = {-1, -1, -1};
  int found = 0;
  for (int i = 0; i < 6 && found < 3; i++) {
    if (enabled[i]) {
      activeIdx[found++] = i;
    }
  }

  for (int row = 0; row < 3; row++) {
    int cfgIdx = (row < found) ? activeIdx[row] : row;
    const char* curPair = (row < found && strlen(pairs[cfgIdx]) > 0) ? pairs[cfgIdx] : defaultPairs[row];
    int rowY = row * 128;

    // Ícone da Moeda
    lv_obj_t *iconBox = lv_obj_create(moeda_container);
    lv_obj_set_size(iconBox, 44, 44);
    lv_obj_set_pos(iconBox, 16, rowY + 42);
    lv_obj_set_style_bg_opa(iconBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(iconBox, 0, 0);
    lv_obj_set_style_pad_all(iconBox, 0, 0);
    lv_obj_clear_flag(iconBox, LV_OBJ_FLAG_SCROLLABLE);
    render_currency_icon(iconBox, curPair);
    moeda_icon_boxes[row] = iconBox;

    // Nome do Par (ex: "BTC/BRL")
    String pStr = String(curPair);
    pStr.replace("-", "/");
    lv_obj_t *pairLbl = lv_label_create(moeda_container);
    lv_label_set_text(pairLbl, pStr.c_str());
    lv_obj_set_style_text_font(pairLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(pairLbl, colWhite, 0);
    lv_obj_set_pos(pairLbl, 70, rowY + 36);
    moeda_pair_labels[row] = pairLbl;

    // Subtítulo (ex: "Bitcoin", "Dolar")
    lv_obj_t *subLbl = lv_label_create(moeda_container);
    lv_label_set_text(subLbl, get_currency_friendly_name(curPair));
    lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(subLbl, colMuted, 0);
    lv_obj_set_pos(subLbl, 70, rowY + 68);
    moeda_sub_labels[row] = subLbl;

    // Valor da Cotação (ex: "R$ 404.208")
    lv_obj_t *valLbl = lv_label_create(moeda_container);
    lv_label_set_text(valLbl, moedaValues[cfgIdx].c_str());
    lv_obj_set_style_text_font(valLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(valLbl, colWhite, 0);
    lv_obj_align(valLbl, LV_ALIGN_TOP_RIGHT, -18, rowY + 36);
    moeda_value_labels[row] = valLbl;

    // Variação Percentual (ex: "-1,01%")
    lv_obj_t *pctLbl = lv_label_create(moeda_container);
    lv_label_set_text(pctLbl, moedaPcts[cfgIdx].c_str());
    lv_obj_set_style_text_font(pctLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pctLbl, moedaPctPos[cfgIdx] ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);
    lv_obj_align(pctLbl, LV_ALIGN_TOP_RIGHT, -18, rowY + 68);
    moeda_pct_labels[row] = pctLbl;

    // Linha Divisória Horizontal entre as moedas
    if (row < 2) {
      lv_obj_t *lineDiv = lv_obj_create(moeda_container);
      lv_obj_set_size(lineDiv, 352, 1);
      lv_obj_set_pos(lineDiv, 16, rowY + 128);
      lv_obj_set_style_bg_color(lineDiv, colBorder, 0);
      lv_obj_set_style_border_width(lineDiv, 0, 0);
    }
  }
}

// Atualização do Relógio, Data e Top Bar a cada segundo
void update_clock(lv_timer_t *timer) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    static const char *weekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
    static const char *months[] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};
    char dateStr[32];
    snprintf(dateStr, sizeof(dateStr), "%s, %02d %s",
             weekdays[timeinfo.tm_wday], timeinfo.tm_mday, months[timeinfo.tm_mon]);

    if (time_label) lv_label_set_text(time_label, timeStr);
    if (date_label) lv_label_set_text(date_label, dateStr);
  }

  if (top_uptime_label) lv_label_set_text(top_uptime_label, get_uptime_str().c_str());
  if (top_ram_label) {
    char ramBuf[32];
    snprintf(ramBuf, sizeof(ramBuf), "RAM: %d KB Livre", (int)(ESP.getFreeHeap() / 1024));
    lv_label_set_text(top_ram_label, ramBuf);
  }
}

// Formatação brasileira de moedas com separador de milhar
String format_currency_value(float val) {
  char buf[32];
  if (val >= 1000.0f) {
    long intVal = (long)val;
    if (intVal >= 1000000) {
      snprintf(buf, sizeof(buf), "R$ %ld.%03ld.%03ld", intVal / 1000000, (intVal % 1000000) / 1000, intVal % 1000);
    } else {
      snprintf(buf, sizeof(buf), "R$ %ld.%03ld", intVal / 1000, intVal % 1000);
    }
  } else {
    snprintf(buf, sizeof(buf), "R$ %.2f", val);
    char *dot = strchr(buf, '.');
    if (dot) *dot = ',';
  }
  return String(buf);
}

// Atualização de Cotação de Moedas
void update_dolar(lv_timer_t *timer) {
  if (WiFi.status() != WL_CONNECTED) return;
  String pairs[6] = {String(gConfig.currency_1), String(gConfig.currency_2), String(gConfig.currency_3), String(gConfig.currency_4), String(gConfig.currency_5), String(gConfig.currency_6)};
  bool enabled[6] = {gConfig.curr1_enabled, gConfig.curr2_enabled, gConfig.curr3_enabled, gConfig.curr4_enabled, gConfig.curr5_enabled, gConfig.curr6_enabled};
  String list = "";
  for (int i = 0; i < 6; i++) {
    if (enabled[i] && pairs[i].length() > 0) {
      if (list.length()) list += ",";
      list += pairs[i];
    }
  }
  if (list.length() == 0) list = "BTC-BRL,USD-BRL,ETH-BRL";

  String url = "https://economia.awesomeapi.com.br/json/last/" + list;
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      int idx = 0;
      for (int i = 0; i < 6 && idx < 3; i++) {
        if (enabled[i]) {
          String key = pairs[i];
          key.replace("-", "");
          if (doc[key].is<JsonObject>()) {
            float bid = doc[key]["bid"].as<float>();
            float pct = doc[key]["pctChange"].as<float>();

            String valStr = format_currency_value(bid);
            moedaValues[i] = valStr;
            if (i == 0) dolarValue = valStr;

            char pctBuf[24];
            bool isPos = (pct >= 0);
            if (isPos) {
              snprintf(pctBuf, sizeof(pctBuf), "+%.2f%%", pct);
            } else {
              snprintf(pctBuf, sizeof(pctBuf), "%.2f%%", pct);
            }
            char *pdot = strchr(pctBuf, '.');
            if (pdot) *pdot = ',';

            moedaPcts[i] = String(pctBuf);
            moedaPctPos[i] = isPos;

            if (moeda_value_labels[idx]) {
              lv_label_set_text(moeda_value_labels[idx], valStr.c_str());
            }
            if (moeda_pct_labels[idx]) {
              lv_label_set_text(moeda_pct_labels[idx], pctBuf);
              lv_obj_set_style_text_color(moeda_pct_labels[idx], isPos ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);
            }
            if (moeda_pair_labels[idx]) {
              String p = pairs[i];
              p.replace("-", "/");
              lv_label_set_text(moeda_pair_labels[idx], p.c_str());
            }
            if (moeda_sub_labels[idx]) {
              lv_label_set_text(moeda_sub_labels[idx], get_currency_friendly_name(pairs[i].c_str()));
            }
            if (moeda_icon_boxes[idx]) {
              render_currency_icon(moeda_icon_boxes[idx], pairs[i].c_str());
            }

            idx++;
          }
        }
      }
    }
  }
  http.end();
}

// Atualização do Clima Atual e Previsão Semanal
void update_weather(lv_timer_t *timer) {
  if (WiFi.status() != WL_CONNECTED) return;

  char url[256];
  snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&daily=weather_code,temperature_2m_max&timezone=auto&forecast_days=5", gConfig.lat, gConfig.lon);
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      int humidity = 64;
      float wind = 17;
      int wcode = 0;

      if (doc["current"].is<JsonObject>()) {
        humidity = doc["current"]["relative_humidity_2m"].as<int>();
        wind = doc["current"]["wind_speed_10m"].as<float>();
        wcode = doc["current"]["weather_code"].as<int>();
      }

      currentWeatherCode = wcode;
      weatherCity = String(gConfig.city);

      char bufHum[32];
      snprintf(bufHum, sizeof(bufHum), "Umidade: %d%%", humidity);
      weatherHumidity = bufHum;

      char bufWind[32];
      snprintf(bufWind, sizeof(bufWind), "Vento: %.0f km/h", wind);
      weatherWind = bufWind;

      if (weather_humidity_label) lv_label_set_text(weather_humidity_label, weatherHumidity.c_str());
      if (weather_wind_label) lv_label_set_text(weather_wind_label, weatherWind.c_str());
      if (weather_city_label) {
        String cUpper = sanitize_for_lvgl(weatherCity);
        cUpper.toUpperCase();
        lv_label_set_text(weather_city_label, cUpper.c_str());
      }
      if (weather_icon_box) {
        render_weather_icon(weather_icon_box, currentWeatherCode);
      }

      // Processa a Previsão Semanal de 5 Dias
      if (doc["daily"].is<JsonObject>()) {
        static const char *weekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
        struct tm timeinfo;
        int curWday = 3; // Default QUA
        if (getLocalTime(&timeinfo)) curWday = timeinfo.tm_wday;

        for (int i = 0; i < 5; i++) {
          int dWday = (curWday + i) % 7;
          weeklyForecast[i].dayName = weekdays[dWday];
          if (doc["daily"]["temperature_2m_max"][i].is<float>()) {
            weeklyForecast[i].tempMax = (int)round(doc["daily"]["temperature_2m_max"][i].as<float>());
          }
          if (doc["daily"]["weather_code"][i].is<int>()) {
            weeklyForecast[i].weatherCode = doc["daily"]["weather_code"][i].as<int>();
          }

          if (forecast_day_labels[i]) lv_label_set_text(forecast_day_labels[i], weeklyForecast[i].dayName.c_str());
          if (forecast_temp_labels[i]) {
            char tBuf[16];
            snprintf(tBuf, sizeof(tBuf), "%d C", weeklyForecast[i].tempMax);
            lv_label_set_text(forecast_temp_labels[i], tBuf);
          }
          if (forecast_icon_boxes[i]) {
            render_mini_weather_icon(forecast_icon_boxes[i], weeklyForecast[i].weatherCode);
          }
        }
      }
    }
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== SMART DASHBOARD BOOT ===");
  loadConfig();

  lv_init();
  tft.init();
  tft.setRotation(0);
  tft.setBrightness(gConfig.brightness);
  tft.fillScreen(TFT_BLACK);

  buf1 = (lv_color_t *)heap_caps_malloc(800 * BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf1) buf1 = (lv_color_t *)heap_caps_malloc(800 * BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  buf2 = nullptr;
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 800 * BUF_LINES);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 800;
  disp_drv.ver_res = 480;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touch_read;
  lv_indev_drv_register(&indev_drv);

  create_ui();

  // AP de configuracao SEMPRE ligado
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("Painel-Config", "12345678");

  webServerInit();

  if (gConfig.wifi_ssid[0] != '\0') {
    Serial.printf("[WiFi] Conectando em '%s' ...\n", gConfig.wifi_ssid);
    WiFi.begin(gConfig.wifi_ssid, gConfig.wifi_pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      lv_timer_handler();
      webServerLoop();
      delay(10);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (top_wifi_label) {
      char buf[64];
      snprintf(buf, sizeof(buf), "WiFi: %s (%ddBm)", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
      lv_label_set_text(top_wifi_label, buf);
    }
  }

  configTime(gConfig.tz_offset * 3600, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  int ntpWait = 0;
  while (!getLocalTime(&timeinfo) && ntpWait < 10) {
    lv_timer_handler();
    webServerLoop();
    delay(500);
    ntpWait++;
  }

  lv_timer_create(update_clock, 1000, NULL);
  lv_timer_create(update_dolar, gConfig.dolar_interval * 1000, NULL);
  lv_timer_create(update_weather, gConfig.weather_interval * 1000, NULL);
  update_dolar(NULL);
  update_weather(NULL);
  update_clock(NULL);

  otaInit();
}

void loop() {
  if (gNeedsRebuild) {
    gNeedsRebuild = false;
    create_ui();
    update_dolar(NULL);
    update_weather(NULL);
    update_clock(NULL);
  }
  lv_timer_handler();
  webServerLoop();
  otaLoop();
  delay(5);
}
