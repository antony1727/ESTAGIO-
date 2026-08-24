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
#define BUF_LINES 32 // 800*32 = 25600 px ~50KB - single buffer p/ evitar piscada

static lv_obj_t *time_label;
static lv_obj_t *date_label;
static lv_obj_t *status_label;
static lv_obj_t *wifi_dot;
static lv_obj_t *weather_temp_label;
static lv_obj_t *weather_desc_label;
static lv_obj_t *weather_city_label;

// Suporte a at� 6 moedas - metade da tela em 2 linhas se >3
static lv_obj_t *moeda_cards[6] = {nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
static lv_obj_t *moeda_pair_labels[6] = {nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
static lv_obj_t *moeda_value_labels[6] = {nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
// compatibilidade: dolar_label aponta para moeda 0
static lv_obj_t *dolar_label = nullptr;
static String moedaValues[6] = {"R$ --,--","R$ --,--","R$ --,--","R$ --,--","R$ --,--","R$ --,--"};
String dolarValue = "R$ --,--"; // alias para moedaValues[0] e /api/data
String weatherTemp = "--";
String weatherDesc = "----";
String weatherCity = "Sao Paulo";
volatile bool gNeedsRebuild = false;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  // Lovyan RGB - pushImage faz byteswap correto para LVGL
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

lv_obj_t *make_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_color_t bg) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_color(card, bg, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 16, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 16, 0);
  lv_obj_set_style_shadow_opa(card, 40, 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
  // padding menor para moedas caber BTC inteiro
  lv_obj_set_style_pad_all(card, 10, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

void make_accent_strip(lv_obj_t *parent, lv_coord_t h, lv_color_t color) {
  lv_obj_t *strip = lv_obj_create(parent);
  lv_obj_set_size(strip, 100, h);
  lv_obj_set_pos(strip, 0, 0);
  lv_obj_set_style_bg_color(strip, color, 0);
  lv_obj_set_style_radius(strip, 0, 0);
  lv_obj_set_style_border_width(strip, 0, 0);
}

void create_ui() {
  // Calcula quantas moedas ativas (max 6)
  int cnt = (gConfig.curr1_enabled?1:0) + (gConfig.curr2_enabled?1:0) + (gConfig.curr3_enabled?1:0)
          + (gConfig.curr4_enabled?1:0) + (gConfig.curr5_enabled?1:0) + (gConfig.curr6_enabled?1:0);
  if(cnt==0) cnt=1;
  bool twoRows = (cnt>3); // >3 usa metade da tela (2 linhas) para moedas

  int timeW, climaW, moedaW;
  int moedaH = 300;
  int gap = 12;
  // Para >3 moedas: time e clima no topo, moedas em grid 3x2 na metade direita/inferior
  // Mantemos time e clima sempre vis�veis

  bool light = gConfig.display_light;
  lv_color_t colBg = light? lv_color_hex(0xF1F5F9) : lv_color_hex(0x070A12);
  lv_color_t colBgGrad = light? lv_color_hex(0xFFFFFF) : lv_color_hex(0x0E1420);
  lv_color_t colHeader = light? lv_color_hex(0xFFFFFF) : lv_color_hex(0x06080D);
  lv_color_t colCard = light? lv_color_hex(0xFFFFFF) : lv_color_hex(0x0F1622);
  lv_color_t colBorder = light? lv_color_hex(0xE2E8F0) : lv_color_hex(0x1E2A3A);
  lv_color_t colText = light? lv_color_hex(0x0F172A) : lv_color_hex(0xF8FAFC);
  lv_color_t colMuted = light? lv_color_hex(0x64748B) : lv_color_hex(0x7A8699);

  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, colBg, 0);
  lv_obj_set_style_bg_grad_color(scr, colBgGrad, 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_main_stop(scr, 0, 0);
  lv_obj_set_style_bg_grad_stop(scr, 255, 0);

  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_set_size(header, 800, 84);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, colHeader, 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_border_color(header, colBorder, 0);
  lv_obj_set_style_pad_all(header, 0, 0);

  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "PAINEL FINANCEIRO");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, colText, 0);
  lv_obj_set_style_text_letter_space(title, 2, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 32, 0);

  lv_obj_t *title_bar = lv_obj_create(header);
  lv_obj_set_size(title_bar, 6, 44);
  lv_obj_set_pos(title_bar, 14, 20);
  lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x22D3EE), 0);
  lv_obj_set_style_radius(title_bar, 3, 0);
  lv_obj_set_style_border_width(title_bar, 0, 0);

  lv_obj_t *title_sub = lv_label_create(header);
  lv_label_set_text(title_sub, "Horario e cambio em tempo real");
  lv_obj_set_style_text_font(title_sub, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title_sub, colMuted, 0);
  lv_obj_align(title_sub, LV_ALIGN_LEFT_MID, 32, 30);

  wifi_dot = lv_obj_create(header);
  lv_obj_set_size(wifi_dot, 12, 12);
  lv_obj_set_style_bg_color(wifi_dot, lv_color_hex(0xFF5252), 0);
  lv_obj_set_style_radius(wifi_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(wifi_dot, 0, 0);
  lv_obj_set_style_shadow_width(wifi_dot, 10, 0);
  lv_obj_set_style_shadow_color(wifi_dot, lv_color_hex(0xFF5252), 0);
  lv_obj_align(wifi_dot, LV_ALIGN_RIGHT_MID, -140, 0);

  status_label = lv_label_create(header);
  lv_label_set_text(status_label, "Conectando...");
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(status_label, colMuted, 0);
  lv_obj_align(status_label, LV_ALIGN_RIGHT_MID, -32, 0);

  // LAYOUT: se <=3 moedas -> linha unica proporcional; se >3 -> moedas ocupam metade direita em grid 3x2 (usa metade da tela)
  if(!twoRows){
    // modo 1-3 moedas - linha unica
    int availSingle = 800 - 56 - (2+cnt-1)*gap;
    int totalWeight = 5 + cnt*2;
    int unit = availSingle / totalWeight;
    timeW = unit*3;
    climaW = unit*2;
    moedaW = unit*2;
    int used = timeW + climaW + moedaW*cnt + (2+cnt-1)*gap + 56;
    timeW += (800 - used);
    int x = 28;
    // TIME
    lv_obj_t *time_card = make_card(scr, x, 108, timeW, 300, colCard);
    lv_obj_set_style_border_color(time_card, colBorder, 0);
    lv_obj_set_style_border_width(time_card, 1, 0);
    make_accent_strip(time_card, 4, lv_color_hex(0x22D3EE));
    lv_obj_t *time_caption = lv_label_create(time_card);
    lv_label_set_text(time_caption, "HORARIO LOCAL");
    lv_obj_set_style_text_font(time_caption, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_caption, lv_color_hex(0x22D3EE), 0);
    lv_obj_set_style_text_letter_space(time_caption, 3, 0);
    lv_obj_align(time_caption, LV_ALIGN_TOP_MID, 0, 18);
    time_label = lv_label_create(time_card);
    lv_label_set_text(time_label, "--:--:--");
    lv_obj_set_style_text_font(time_label, timeW>300? &lv_font_montserrat_48 : timeW>220? &lv_font_montserrat_32 : &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(time_label, colText, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 10);
    date_label = lv_label_create(time_card);
    lv_label_set_text(date_label, "");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(date_label, colMuted, 0);
    lv_obj_align(date_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    x += timeW + gap;
    // CLIMA
    lv_obj_t *weather_card = make_card(scr, x, 108, climaW, 300, colCard);
    lv_obj_set_style_border_color(weather_card, colBorder, 0);
    lv_obj_set_style_border_width(weather_card, 1, 0);
    make_accent_strip(weather_card, 4, lv_color_hex(0xFFB300));
    lv_obj_t *weather_caption = lv_label_create(weather_card);
    lv_label_set_text(weather_caption, "CLIMA");
    lv_obj_set_style_text_font(weather_caption, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_caption, lv_color_hex(0xFFB300), 0);
    lv_obj_set_style_text_letter_space(weather_caption, 3, 0);
    lv_obj_align(weather_caption, LV_ALIGN_TOP_MID, 0, 18);
    weather_city_label = lv_label_create(weather_card);
    lv_label_set_text(weather_city_label, weatherCity.c_str());
    lv_obj_set_style_text_font(weather_city_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_city_label, colMuted, 0);
    lv_obj_align(weather_city_label, LV_ALIGN_TOP_MID, 0, 52);
    weather_temp_label = lv_label_create(weather_card);
    lv_label_set_text(weather_temp_label, "--\xC2\xB0");
    lv_obj_set_style_text_font(weather_temp_label, climaW>150? &lv_font_montserrat_48 : &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(weather_temp_label, lv_color_hex(0xFFB300), 0);
    lv_obj_align(weather_temp_label, LV_ALIGN_CENTER, 0, 10);
    weather_desc_label = lv_label_create(weather_card);
    lv_label_set_text(weather_desc_label, "----");
    lv_obj_set_style_text_font(weather_desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_desc_label, colMuted, 0);
    lv_obj_align(weather_desc_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    x += climaW + gap;
    // MOEDAS
    const char* pairs[6] = {gConfig.currency_1, gConfig.currency_2, gConfig.currency_3, gConfig.currency_4, gConfig.currency_5, gConfig.currency_6};
    bool enabled[6] = {gConfig.curr1_enabled, gConfig.curr2_enabled, gConfig.curr3_enabled, gConfig.curr4_enabled, gConfig.curr5_enabled, gConfig.curr6_enabled};
    lv_color_t accents[6] = {lv_color_hex(0x00E676), lv_color_hex(0x2979FF), lv_color_hex(0xFFAB00), lv_color_hex(0xFF4081), lv_color_hex(0xE040FB), lv_color_hex(0x18FFFF)};
    int idx=0;
    for(int i=0;i<6 && idx<cnt;i++){
      if(!enabled[i]) continue;
      lv_obj_t *card = make_card(scr, x, 108, moedaW, 300, colCard);
      // padding menor para BTC caber inteiro (R$ 401322)
      lv_obj_set_style_pad_all(card, 6, 0);
      lv_obj_set_style_border_color(card, colBorder, 0);
      lv_obj_set_style_border_width(card, 1, 0);
      make_accent_strip(card, 4, accents[i]);
      moeda_cards[idx]=card;
      lv_obj_t *cap = lv_label_create(card);
      lv_label_set_text(cap, cnt==1? "CAMBIO" : pairs[i]);
      lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(cap, accents[i], 0);
      lv_obj_set_style_text_letter_space(cap, 1, 0);
      lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 16);
      String p = String(pairs[i]); p.replace("-"," / ");
      lv_obj_t *pairLbl = lv_label_create(card);
      lv_label_set_text(pairLbl, p.c_str());
      lv_obj_set_style_text_font(pairLbl, moedaW>130? &lv_font_montserrat_14 : &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(pairLbl, colMuted, 0);
      lv_obj_align(pairLbl, LV_ALIGN_TOP_MID, 0, 44);
      moeda_pair_labels[idx]=pairLbl;
      lv_obj_t *val = lv_label_create(card);
      lv_label_set_text(val, moedaValues[i].c_str());
      // BTC precisa caber: usa fonte menor se valor longo
      int len = moedaValues[i].length();
      const lv_font_t* f = &lv_font_montserrat_20;
      if(moedaW>140) f = len>8? &lv_font_montserrat_24 : &lv_font_montserrat_32;
      else if(moedaW>110) f = len>8? &lv_font_montserrat_20 : &lv_font_montserrat_24;
      else f = &lv_font_montserrat_16;
      lv_obj_set_style_text_font(val, f, 0);
      lv_obj_set_style_text_color(val, accents[i], 0);
      lv_label_set_long_mode(val, LV_LABEL_LONG_SCROLL_CIRCULAR);
      lv_obj_set_width(val, moedaW-12);
      lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_align(val, LV_ALIGN_CENTER, 0, 18);
      moeda_value_labels[idx]=val;
      if(idx==0) dolar_label = val;
      x += moedaW + gap;
      idx++;
    }
    for(int i=idx;i<6;i++){ moeda_cards[i]=nullptr; moeda_pair_labels[i]=nullptr; moeda_value_labels[i]=nullptr; }
  } else {
    // modo 4-6 moedas: metade da tela para moedas (direita), time+clima esquerda
    timeW = 340; // esquerda fixa
    climaW = 380; // na verdade clima fica topo direita, mas calculamos
    int rightW = 800 - 28 - timeW - gap - 28; // 392
    int xTime = 28;
    int xRight = xTime + timeW + gap;
    // TIME grande esquerda 300 altura
    lv_obj_t *time_card = make_card(scr, xTime, 108, timeW, 300, colCard);
    lv_obj_set_style_border_color(time_card, colBorder, 0);
    lv_obj_set_style_border_width(time_card, 1, 0);
    make_accent_strip(time_card, 4, lv_color_hex(0x22D3EE));
    lv_obj_t *time_caption = lv_label_create(time_card);
    lv_label_set_text(time_caption, "HORARIO LOCAL");
    lv_obj_set_style_text_font(time_caption, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_caption, lv_color_hex(0x22D3EE), 0);
    lv_obj_set_style_text_letter_space(time_caption, 3, 0);
    lv_obj_align(time_caption, LV_ALIGN_TOP_MID, 0, 18);
    time_label = lv_label_create(time_card);
    lv_label_set_text(time_label, "--:--:--");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, colText, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 10);
    date_label = lv_label_create(time_card);
    lv_label_set_text(date_label, "");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(date_label, colMuted, 0);
    lv_obj_align(date_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    // CLIMA topo direita
    lv_obj_t *weather_card = make_card(scr, xRight, 108, rightW, 140, colCard);
    lv_obj_set_style_border_color(weather_card, colBorder, 0);
    lv_obj_set_style_border_width(weather_card, 1, 0);
    make_accent_strip(weather_card, 4, lv_color_hex(0xFFB300));
    lv_obj_t *weather_caption = lv_label_create(weather_card);
    lv_label_set_text(weather_caption, "CLIMA");
    lv_obj_set_style_text_font(weather_caption, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_caption, lv_color_hex(0xFFB300), 0);
    lv_obj_set_style_text_letter_space(weather_caption, 3, 0);
    lv_obj_align(weather_caption, LV_ALIGN_TOP_MID, 0, 12);
    weather_city_label = lv_label_create(weather_card);
    lv_label_set_text(weather_city_label, weatherCity.c_str());
    lv_obj_set_style_text_font(weather_city_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_city_label, colMuted, 0);
    lv_obj_align(weather_city_label, LV_ALIGN_TOP_MID, 0, 36);
    weather_temp_label = lv_label_create(weather_card);
    lv_label_set_text(weather_temp_label, "--\xC2\xB0");
    lv_obj_set_style_text_font(weather_temp_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(weather_temp_label, lv_color_hex(0xFFB300), 0);
    lv_obj_align(weather_temp_label, LV_ALIGN_LEFT_MID, 16, 14);
    weather_desc_label = lv_label_create(weather_card);
    lv_label_set_text(weather_desc_label, "----");
    lv_obj_set_style_text_font(weather_desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_desc_label, colMuted, 0);
    lv_obj_align(weather_desc_label, LV_ALIGN_RIGHT_MID, -16, 14);
    // MOEDAS grid 3x2 na metade direita inferior (usa s� metade da tela)
    const char* pairs[6] = {gConfig.currency_1, gConfig.currency_2, gConfig.currency_3, gConfig.currency_4, gConfig.currency_5, gConfig.currency_6};
    bool enabled[6] = {gConfig.curr1_enabled, gConfig.curr2_enabled, gConfig.curr3_enabled, gConfig.curr4_enabled, gConfig.curr5_enabled, gConfig.curr6_enabled};
    lv_color_t accents[6] = {lv_color_hex(0x00E676), lv_color_hex(0x2979FF), lv_color_hex(0xFFAB00), lv_color_hex(0xFF4081), lv_color_hex(0xE040FB), lv_color_hex(0x18FFFF)};
    int cols=3; int rows = (cnt+2)/3; // 2 linhas para 4-6
    int moedaGap = 8;
    moedaW = (rightW - (cols-1)*moedaGap)/cols; // ~125
    moedaH = (148 - (rows-1)*moedaGap)/rows; // ~70 para 2 linhas
    if(rows==1) moedaH = 148;
    int baseY = 108+140+12; // 260
    int idx=0;
    for(int i=0;i<6 && idx<cnt;i++){
      if(!enabled[i]) continue;
      int col = idx % cols;
      int row = idx / cols;
      int mx = xRight + col*(moedaW+moedaGap);
      int my = baseY + row*(moedaH+moedaGap);
      lv_obj_t *card = make_card(scr, mx, my, moedaW, moedaH, colCard);
      lv_obj_set_style_pad_all(card, 6, 0);
      lv_obj_set_style_border_color(card, colBorder, 0);
      lv_obj_set_style_border_width(card, 1, 0);
      make_accent_strip(card, 3, accents[i]);
      moeda_cards[idx]=card;
      lv_obj_t *cap = lv_label_create(card);
      lv_label_set_text(cap, pairs[i]);
      lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(cap, accents[i], 0);
      lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 6);
      String p = String(pairs[i]); p.replace("-","/");
      lv_obj_t *pairLbl = lv_label_create(card);
      lv_label_set_text(pairLbl, p.c_str());
      lv_obj_set_style_text_font(pairLbl, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(pairLbl, colMuted, 0);
      lv_obj_align(pairLbl, LV_ALIGN_TOP_MID, 0, 20);
      moeda_pair_labels[idx]=pairLbl;
      lv_obj_t *val = lv_label_create(card);
      lv_label_set_text(val, moedaValues[i].c_str());
      // ajuste para BTC caber inteiro
      int len = moedaValues[i].length();
      const lv_font_t* f = len>9? &lv_font_montserrat_14 : len>7? &lv_font_montserrat_16 : &lv_font_montserrat_20;
      lv_obj_set_style_text_font(val, f, 0);
      lv_obj_set_style_text_color(val, accents[i], 0);
      lv_label_set_long_mode(val, LV_LABEL_LONG_SCROLL_CIRCULAR);
      lv_obj_set_width(val, moedaW-8);
      lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_align(val, LV_ALIGN_BOTTOM_MID, 0, -6);
      moeda_value_labels[idx]=val;
      if(idx==0) dolar_label = val;
      idx++;
    }
    for(int i=idx;i<6;i++){ moeda_cards[i]=nullptr; moeda_pair_labels[i]=nullptr; moeda_value_labels[i]=nullptr; }
  }

  lv_obj_t *footer = lv_label_create(scr);
  char fbuf[80];
  snprintf(fbuf,sizeof(fbuf),"Atualizado %ds | %d moeda(s) | %s | v%s", gConfig.dolar_interval, cnt, gConfig.city, FIRMWARE_VERSION);
  lv_label_set_text(footer, fbuf);
  lv_obj_set_style_text_font(footer, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(footer, colMuted, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void update_clock(lv_timer_t *timer) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    static const char *weekdays[] = {"Domingo", "Segunda", "Terca", "Quarta", "Quinta", "Sexta", "Sabado"};
    static const char *months[] = {"Jan", "Fev", "Mar", "Abr", "Mai", "Jun", "Jul", "Ago", "Set", "Out", "Nov", "Dez"};
    char dateStr[64];
    snprintf(dateStr, sizeof(dateStr), "%s, %02d de %s de %d",
             weekdays[timeinfo.tm_wday], timeinfo.tm_mday, months[timeinfo.tm_mon], timeinfo.tm_year + 1900);

    lv_label_set_text(time_label, timeStr);
    lv_label_set_text(date_label, dateStr);
  }
}

void update_dolar(lv_timer_t *timer) {
  if (WiFi.status() != WL_CONNECTED) return;
  String pairs[6] = {String(gConfig.currency_1), String(gConfig.currency_2), String(gConfig.currency_3), String(gConfig.currency_4), String(gConfig.currency_5), String(gConfig.currency_6)};
  bool enabled[6] = {gConfig.curr1_enabled, gConfig.curr2_enabled, gConfig.curr3_enabled, gConfig.curr4_enabled, gConfig.curr5_enabled, gConfig.curr6_enabled};
  String list = "";
  for(int i=0;i<6;i++) if(enabled[i]) { if(list.length()) list+=",";
    list += pairs[i];
  }
  if(list.length()==0) return;
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
      int idx=0;
      for(int i=0;i<6;i++) if(enabled[i]){
        String key = pairs[i]; key.replace("-","");
        if(doc[key].is<JsonObject>()){
          const char *bid = doc[key]["bid"];
          if(bid){
            // formata BTC sem travar: mantem 2 decimais mas cabe
            String val = String("R$ ") + bid;
            // para BTC remove decimais extras se muito longo
            if(val.length()>10) {
              float f = doc[key]["bid"].as<float>();
              if(f>10000) val = String("R$ ") + String((int)f);
            }
            moedaValues[i] = val;
            if(i==0) dolarValue = val;
            if(idx<6 && moeda_value_labels[idx]) {
              lv_label_set_text(moeda_value_labels[idx], val.c_str());
              // ajusta fonte para caber
              if(val.length()>9) lv_obj_set_style_text_font(moeda_value_labels[idx], &lv_font_montserrat_14, 0);
              else if(val.length()>7) lv_obj_set_style_text_font(moeda_value_labels[idx], &lv_font_montserrat_16, 0);
            }
            if(idx<6 && moeda_pair_labels[idx]){
              String p = pairs[i]; p.replace("-"," / ");
              lv_label_set_text(moeda_pair_labels[idx], p.c_str());
            }
            Serial.println(pairs[i] + ": " + val);
          }
          idx++;
        }
      }
    } else {
      Serial.println("Erro ao parsear JSON dolar");
    }
  } else {
    Serial.printf("Erro HTTP dolar %d list %s\n", httpCode, list.c_str());
  }
  http.end();
}

void update_weather(lv_timer_t *timer) {
  if (WiFi.status() != WL_CONNECTED) return;

  char url[160];
  snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true", gConfig.lat, gConfig.lon);
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      float temp = doc["current_weather"]["temperature"].as<float>();
      int wcode = doc["current_weather"]["weathercode"].as<int>();

      static const char *descs[] = {"Sol", "Predom. sol", "Parcial. nuvens", "Nublado", "Nevoeiro", "Chuvisco", "Chuva", "Neve", "Trovoadas"};
      const char *desc = "N/A";
      if (wcode == 0) desc = descs[0];
      else if (wcode == 1 || wcode == 2) desc = descs[1];
      else if (wcode == 3) desc = descs[2];
      else if (wcode == 45 || wcode == 48) desc = descs[3];
      else if (wcode >= 51 && wcode <= 57) desc = descs[4];
      else if (wcode >= 61 && wcode <= 67) desc = descs[5];
      else if (wcode >= 71 && wcode <= 77) desc = descs[6];
      else if (wcode >= 95) desc = descs[7];

      char buf[16];
      snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", temp);
      weatherTemp = buf;
      weatherDesc = desc;
      weatherCity = String(gConfig.city);

      lv_label_set_text(weather_temp_label, weatherTemp.c_str());
      lv_label_set_text(weather_desc_label, weatherDesc.c_str());
      lv_label_set_text(weather_city_label, weatherCity.c_str());
      Serial.println("Clima: " + weatherTemp + " " + weatherDesc + " @" + weatherCity);
    } else {
      Serial.println("Erro ao parsear clima JSON");
    }
  } else {
    Serial.printf("Erro HTTP clima: %d\n", httpCode);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== PAINEL FINANCEIRO boot ===");
  loadConfig();
  Serial.printf("[WiFi] Config SSID='%s' PASS len=%d\n", gConfig.wifi_ssid, strlen(gConfig.wifi_pass));

  lv_init();
  tft.init();
  tft.setRotation(0);
  tft.setBrightness(gConfig.brightness);
  tft.fillScreen(TFT_BLACK);

  // Single buffer no interno reduz piscada (double PSRAM causa tearing no RGB)
  buf1 = (lv_color_t *)heap_caps_malloc(800 * BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf1) buf1 = (lv_color_t *)heap_caps_malloc(800 * BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  buf2 = nullptr; // single buffer = sem tearing
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

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.printf("[WiFi] Conectando em '%s' ...\n", gConfig.wifi_ssid);
  WiFi.begin(gConfig.wifi_ssid, gConfig.wifi_pass);

  unsigned long start = millis();
  int dot=0;
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    lv_timer_handler();
    if(millis()%1000<20){ Serial.print("."); dot++; if(dot%60==0) Serial.println(); }
    delay(10);
  }
  Serial.println();
  Serial.printf("[WiFi] status=%d (%s)\n", WiFi.status(), WiFi.status()==WL_CONNECTED?"OK":"FALHA");
  if(WiFi.status()!=WL_CONNECTED){
    WiFi.printDiag(Serial);
    Serial.println("[WiFi] Scan redes proximas:");
    int n=WiFi.scanNetworks();
    Serial.printf("[WiFi] %d redes encontradas\n", n);
    for(int i=0;i<n && i<10;i++) Serial.printf("  %d: %s (%d dBm) %s\n",i,WiFi.SSID(i).c_str(),WiFi.RSSI(i),WiFi.encryptionType(i)==WIFI_AUTH_OPEN?"open":"wpa");
    WiFi.scanDelete();
  }

 if (WiFi.status() == WL_CONNECTED) {
    lv_obj_set_style_bg_color(wifi_dot, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_shadow_color(wifi_dot, lv_color_hex(0x00E676), 0);
    char buf[64];
    snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
    lv_label_set_text(status_label, buf);
    Serial.println(String("WiFi conectado! IP: ") + WiFi.localIP().toString());
    webServerInit();
  } else {
    lv_obj_set_style_bg_color(wifi_dot, lv_color_hex(0xFF5252), 0);
    lv_obj_set_style_shadow_color(wifi_dot, lv_color_hex(0xFF5252), 0);
    lv_label_set_text(status_label, "Falha WiFi - AP 192.168.4.1");
    Serial.println("Falha WiFi - iniciando AP para config");
    WiFi.softAP("Painel-Config", "12345678");
    Serial.println(String("AP IP: ") + WiFi.softAPIP().toString());
    webServerInit();
  }

  configTime(gConfig.tz_offset * 3600, 0, "pool.ntp.org", "time.nist.gov");
  // inicializa labels clima/cidade com config salva
  weatherCity = String(gConfig.city);
  lv_label_set_text(weather_city_label, weatherCity.c_str());
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    lv_timer_handler();
    delay(500);
  }
  Serial.println("Tempo sincronizado!");

  lv_timer_create(update_clock, 1000, NULL);
  lv_timer_create(update_dolar, gConfig.dolar_interval * 1000, NULL);
  lv_timer_create(update_weather, gConfig.weather_interval * 1000, NULL);
  update_dolar(NULL);
  update_weather(NULL);

  // OTA auto
  otaInit();
  if(WiFi.status()==WL_CONNECTED){
    // verifica 15s apos boot e a cada 6h
    lv_timer_create([](lv_timer_t* t){ otaCheck(true); }, 15000, NULL);
    lv_timer_create([](lv_timer_t* t){ otaCheck(true); }, 6*3600*1000, NULL);
    // se houver update, baixa automaticamente (opcional: comentar para manual)
    lv_timer_create([](lv_timer_t* t){
      if(gOta.latest.length() && gOta.downloadUrl.length() && gOta.state==OTA_NO_UPDATE){
        int cur = FIRMWARE_VERSION_CODE;
        String tag = gOta.latest; tag.replace("v",""); int maj=0,min=0,pat=0; sscanf(tag.c_str(),"%d.%d.%d",&maj,&min,&pat); int latest = maj*100+min*10+pat;
        if(latest > cur){
          Serial.println("[OTA] auto update disponivel, baixando em background...");
          otaRequestUpdate(); // agenda download na task dedicada (nao bloqueia)
        }
      }
    }, 30000, NULL);
  }
}

void loop() {
  if(gNeedsRebuild){
    gNeedsRebuild = false;
    create_ui();
    // atualiza dados nos novos labels
    update_dolar(NULL);
    update_weather(NULL);
    // for�a status wifi
    if(WiFi.status()==WL_CONNECTED){
      char buf[64]; snprintf(buf,sizeof(buf),"IP: %s",WiFi.localIP().toString().c_str());
      lv_label_set_text(status_label, buf);
    }
  }
  lv_timer_handler();
  webServerLoop();
  delay(5);
}
