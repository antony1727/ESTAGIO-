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

// UI Widgets - Left Panel
static lv_obj_t *time_label = nullptr;
static lv_obj_t *date_label = nullptr;
static lv_obj_t *status_label = nullptr;
static lv_obj_t *wifi_dot = nullptr;
static lv_obj_t *weather_city_label = nullptr;
static lv_obj_t *weather_temp_label = nullptr;
static lv_obj_t *weather_desc_label = nullptr;
static lv_obj_t *weather_humidity_label = nullptr;
static lv_obj_t *weather_wind_label = nullptr;
static lv_obj_t *weather_icon_box = nullptr;

// UI Widgets - Right Panel (Moedas)
static lv_obj_t *moeda_cards[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *moeda_pair_labels[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *moeda_sub_labels[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *moeda_value_labels[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *moeda_pct_labels[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *moeda_icon_boxes[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

// Compatibilidade com web_server.cpp
static lv_obj_t *dolar_label = nullptr;
static String moedaValues[6] = {"R$ --,--", "R$ --,--", "R$ --,--", "R$ --,--", "R$ --,--", "R$ --,--"};
static String moedaPcts[6] = {"--", "--", "--", "--", "--", "--"};
static bool moedaPctPos[6] = {true, true, true, true, true, true};
String dolarValue = "R$ --,--"; // alias para moedaValues[0] e /api/data
String weatherTemp = "--";
String weatherDesc = "----";
String weatherCity = "Sao Paulo";
String weatherHumidity = "Humidity: --%";
String weatherWind = "Wind: -- km/h";
int currentWeatherCode = 0;
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

// Helpers para obter nomes amigáveis das moedas em Português
const char* get_currency_friendly_name(const char* pair) {
  if (strstr(pair, "USD")) return "Dólar";
  if (strstr(pair, "EUR")) return "Euro";
  if (strstr(pair, "BTC")) return "Bitcoin";
  if (strstr(pair, "ETH")) return "Ethereum";
  if (strstr(pair, "GBP")) return "Libra";
  if (strstr(pair, "JPY")) return "Iene";
  if (strstr(pair, "CAD")) return "Dólar Can.";
  if (strstr(pair, "CHF")) return "Franco Suíço";
  if (strstr(pair, "ARS")) return "Peso Arg.";
  if (strstr(pair, "USDT")) return "Tether";
  if (strstr(pair, "SOL")) return "Solana";
  return "Câmbio";
}

// Renderizador dos ícones de Moedas / Bandeiras
void render_currency_icon(lv_obj_t *parent, const char* pair) {
  lv_obj_clean(parent);

  if (strstr(pair, "USD")) {
    // Bandeira dos EUA: listras vermelhas/brancas + cantão azul com estrela
    lv_obj_t *flag = lv_obj_create(parent);
    lv_obj_set_size(flag, 42, 28);
    lv_obj_set_pos(flag, 0, 0);
    lv_obj_set_style_bg_color(flag, lv_color_hex(0xDC2626), 0); // Vermelho
    lv_obj_set_style_radius(flag, 4, 0);
    lv_obj_set_style_border_width(flag, 1, 0);
    lv_obj_set_style_border_color(flag, lv_color_hex(0x475569), 0);
    lv_obj_set_style_pad_all(flag, 0, 0);
    lv_obj_clear_flag(flag, LV_OBJ_FLAG_SCROLLABLE);

    // Listras brancas
    for (int s = 0; s < 3; s++) {
      lv_obj_t *stripe = lv_obj_create(flag);
      lv_obj_set_size(stripe, 42, 4);
      lv_obj_set_pos(stripe, 0, 4 + s * 8);
      lv_obj_set_style_bg_color(stripe, lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_radius(stripe, 0, 0);
      lv_obj_set_style_border_width(stripe, 0, 0);
    }

    // Cantão azul
    lv_obj_t *canton = lv_obj_create(flag);
    lv_obj_set_size(canton, 20, 15);
    lv_obj_set_pos(canton, 0, 0);
    lv_obj_set_style_bg_color(canton, lv_color_hex(0x1E3A8A), 0);
    lv_obj_set_style_radius(canton, 0, 0);
    lv_obj_set_style_border_width(canton, 0, 0);

    // Estrelas (pontos brancos)
    lv_obj_t *star = lv_obj_create(canton);
    lv_obj_set_size(star, 4, 4);
    lv_obj_align(star, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(star, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(star, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(star, 0, 0);
  }
  else if (strstr(pair, "EUR")) {
    // Bandeira da União Europeia: fundo azul + círculo de estrelas amarelas
    lv_obj_t *flag = lv_obj_create(parent);
    lv_obj_set_size(flag, 42, 28);
    lv_obj_set_pos(flag, 0, 0);
    lv_obj_set_style_bg_color(flag, lv_color_hex(0x003399), 0); // Azul UE
    lv_obj_set_style_radius(flag, 4, 0);
    lv_obj_set_style_border_width(flag, 1, 0);
    lv_obj_set_style_border_color(flag, lv_color_hex(0x475569), 0);
    lv_obj_set_style_pad_all(flag, 0, 0);
    lv_obj_clear_flag(flag, LV_OBJ_FLAG_SCROLLABLE);

    // Anel de estrelas / símbolo central
    lv_obj_t *ring = lv_obj_create(flag);
    lv_obj_set_size(ring, 16, 16);
    lv_obj_align(ring, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0xFBBF24), 0);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *dot = lv_obj_create(flag);
    lv_obj_set_size(dot, 4, 4);
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xFBBF24), 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
  }
  else if (strstr(pair, "BTC")) {
    // Emblema Bitcoin: círculo laranja com "₿" ou "B"
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_set_size(circle, 38, 38);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0xF7931A), 0); // Laranja Bitcoin
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(circle, 0, 0);
    lv_obj_set_style_shadow_width(circle, 10, 0);
    lv_obj_set_style_shadow_color(circle, lv_color_hex(0xF7931A), 0);
    lv_obj_set_style_shadow_opa(circle, 80, 0);
    lv_obj_set_style_pad_all(circle, 0, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sym = lv_label_create(circle);
    lv_label_set_text(sym, "B");
    lv_obj_set_style_text_font(sym, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sym, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(sym, LV_ALIGN_CENTER, 0, 0);
  }
  else if (strstr(pair, "ETH")) {
    // Emblema Ethereum: círculo azul/roxo com "Ξ"
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_set_size(circle, 38, 38);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0x627EEA), 0);
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
  else {
    // Genérico
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_set_size(circle, 38, 38);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0x3B82F6), 0);
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

// Renderizador do Ícone de Clima (Sol atrás de nuvem com estética vetorizada)
void render_weather_icon(lv_obj_t *parent, int wcode) {
  if (!parent) return;
  lv_obj_clean(parent);

  bool isSunnyOnly = (wcode == 0);
  bool isCloudyOnly = (wcode == 3 || wcode == 45 || wcode == 48);
  bool isRain = (wcode >= 51 && wcode <= 67) || (wcode >= 80 && wcode <= 82);
  bool isThunder = (wcode >= 95);

  // 1. Sol (amarelo com brilho/sombra)
  if (!isCloudyOnly && !isThunder) {
    lv_obj_t *sun = lv_obj_create(parent);
    int sunSize = isSunnyOnly ? 44 : 32;
    lv_obj_set_size(sun, sunSize, sunSize);
    lv_obj_set_pos(sun, isSunnyOnly ? 12 : 26, isSunnyOnly ? 6 : 2);
    lv_obj_set_style_bg_color(sun, lv_color_hex(0xFBBF24), 0);
    lv_obj_set_style_radius(sun, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sun, 0, 0);
    lv_obj_set_style_shadow_width(sun, 16, 0);
    lv_obj_set_style_shadow_color(sun, lv_color_hex(0xF59E0B), 0);
    lv_obj_set_style_shadow_opa(sun, 180, 0);
    lv_obj_clear_flag(sun, LV_OBJ_FLAG_SCROLLABLE);

    // Raios do sol (pequenos detalhes sutis)
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

  // 2. Nuvem (composta por formas brancas/cinza suave sobrepostas)
  if (!isSunnyOnly) {
    lv_color_t cloudCol = (isThunder || (wcode >= 61 && wcode <= 67)) ? lv_color_hex(0x94A3B8) : lv_color_hex(0xE2E8F0);

    // Base alongada da nuvem
    lv_obj_t *cBase = lv_obj_create(parent);
    lv_obj_set_size(cBase, 48, 22);
    lv_obj_set_pos(cBase, 2, 24);
    lv_obj_set_style_bg_color(cBase, cloudCol, 0);
    lv_obj_set_style_radius(cBase, 11, 0);
    lv_obj_set_style_border_width(cBase, 0, 0);
    lv_obj_set_style_shadow_width(cBase, 8, 0);
    lv_obj_set_style_shadow_color(cBase, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(cBase, 60, 0);
    lv_obj_clear_flag(cBase, LV_OBJ_FLAG_SCROLLABLE);

    // Cúpula 1 da nuvem (esquerda/meio)
    lv_obj_t *cDome1 = lv_obj_create(parent);
    lv_obj_set_size(cDome1, 24, 24);
    lv_obj_set_pos(cDome1, 10, 12);
    lv_obj_set_style_bg_color(cDome1, cloudCol, 0);
    lv_obj_set_style_radius(cDome1, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(cDome1, 0, 0);
    lv_obj_clear_flag(cDome1, LV_OBJ_FLAG_SCROLLABLE);

    // Cúpula 2 da nuvem (direita)
    lv_obj_t *cDome2 = lv_obj_create(parent);
    lv_obj_set_size(cDome2, 18, 18);
    lv_obj_set_pos(cDome2, 28, 16);
    lv_obj_set_style_bg_color(cDome2, cloudCol, 0);
    lv_obj_set_style_radius(cDome2, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(cDome2, 0, 0);
    lv_obj_clear_flag(cDome2, LV_OBJ_FLAG_SCROLLABLE);
  }

  // 3. Gotas de chuva ou raio
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

// Criação da Interface Principal (Design Fiel à Imagem de Referência)
void create_ui() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);

  // Paleta de Cores Moderna Dark Dashboard
  lv_color_t colBg = lv_color_hex(0x0A0F1D);       // Fundo escuro profundo
  lv_color_t colCard = lv_color_hex(0x0F172A);     // Fundo do container esquerdo
  lv_color_t colCardRight = lv_color_hex(0x111C2E);// Fundo dos cards de moedas
  lv_color_t colBorder = lv_color_hex(0x1E293B);   // Borda sutil elegante
  lv_color_t colHeaderGold = lv_color_hex(0xF6C343); // Amarelo/Dourado do título
  lv_color_t colWhite = lv_color_hex(0xFFFFFF);    // Branco principal
  lv_color_t colMuted = lv_color_hex(0x94A3B8);    // Cinza suave para legendas
  lv_color_t colDesc = lv_color_hex(0xCBD5E1);     // Texto de descrição do clima

  lv_obj_set_style_bg_color(scr, colBg, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // ==========================================
  // PAINEL ESQUERDO: RELÓGIO + CLIMA (364 x 444)
  // ==========================================
  lv_obj_t *left_panel = lv_obj_create(scr);
  lv_obj_set_pos(left_panel, 18, 18);
  lv_obj_set_size(left_panel, 364, 444);
  lv_obj_set_style_bg_color(left_panel, colCard, 0);
  lv_obj_set_style_bg_opa(left_panel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(left_panel, 16, 0);
  lv_obj_set_style_border_width(left_panel, 1, 0);
  lv_obj_set_style_border_color(left_panel, colBorder, 0);
  lv_obj_set_style_pad_all(left_panel, 12, 0);
  lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

  // 1. Data (ex: "TER, 24 OUT")
  date_label = lv_label_create(left_panel);
  lv_label_set_text(date_label, "TER, 24 OUT");
  lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(date_label, colMuted, 0);
  lv_obj_set_style_text_letter_space(date_label, 2, 0);
  lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 16);

  // 2. Relógio Digital Grande (ex: "14:38")
  time_label = lv_label_create(left_panel);
  lv_label_set_text(time_label, "--:--");
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(time_label, colWhite, 0);
  lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 48);

  // 3. Linha Divisória Horizontal Sutil
  lv_obj_t *divider = lv_obj_create(left_panel);
  lv_obj_set_size(divider, 290, 1);
  lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 132);
  lv_obj_set_style_bg_color(divider, colBorder, 0);
  lv_obj_set_style_border_width(divider, 0, 0);

  // 4. Nome da Cidade (ex: "LAVRAS, MG")
  weather_city_label = lv_label_create(left_panel);
  String cUpper = weatherCity;
  cUpper.toUpperCase();
  lv_label_set_text(weather_city_label, cUpper.c_str());
  lv_obj_set_style_text_font(weather_city_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(weather_city_label, colWhite, 0);
  lv_obj_set_style_text_letter_space(weather_city_label, 1, 0);
  lv_obj_align(weather_city_label, LV_ALIGN_TOP_MID, 0, 150);

  // 5. Linha do Ícone de Clima e Temperatura Grande ("27°C")
  weather_icon_box = lv_obj_create(left_panel);
  lv_obj_set_size(weather_icon_box, 70, 58);
  lv_obj_set_pos(weather_icon_box, 36, 195);
  lv_obj_set_style_bg_opa(weather_icon_box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(weather_icon_box, 0, 0);
  lv_obj_set_style_pad_all(weather_icon_box, 0, 0);
  lv_obj_clear_flag(weather_icon_box, LV_OBJ_FLAG_SCROLLABLE);
  render_weather_icon(weather_icon_box, currentWeatherCode);

  weather_temp_label = lv_label_create(left_panel);
  lv_label_set_text(weather_temp_label, "--°C");
  lv_obj_set_style_text_font(weather_temp_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(weather_temp_label, colWhite, 0);
  lv_obj_set_pos(weather_temp_label, 125, 195);

  // 6. Condição Climática (ex: "Parcialmente Nublado")
  weather_desc_label = lv_label_create(left_panel);
  lv_label_set_text(weather_desc_label, weatherDesc.c_str());
  lv_obj_set_style_text_font(weather_desc_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(weather_desc_label, colDesc, 0);
  lv_obj_set_pos(weather_desc_label, 125, 268);

  // 7. Umidade (ex: "Humidity: 64%")
  weather_humidity_label = lv_label_create(left_panel);
  lv_label_set_text(weather_humidity_label, weatherHumidity.c_str());
  lv_obj_set_style_text_font(weather_humidity_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_humidity_label, colMuted, 0);
  lv_obj_set_pos(weather_humidity_label, 125, 298);

  // 8. Vento (ex: "Wind: 14 km/h")
  weather_wind_label = lv_label_create(left_panel);
  lv_label_set_text(weather_wind_label, weatherWind.c_str());
  lv_obj_set_style_text_font(weather_wind_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_wind_label, colMuted, 0);
  lv_obj_set_pos(weather_wind_label, 125, 324);

  // 9. Indicador Discreto de WiFi / Status no Rodapé do Painel Esquerdo
  wifi_dot = lv_obj_create(left_panel);
  lv_obj_set_size(wifi_dot, 8, 8);
  lv_obj_set_style_bg_color(wifi_dot, WiFi.status() == WL_CONNECTED ? lv_color_hex(0x00E676) : lv_color_hex(0xFF5252), 0);
  lv_obj_set_style_radius(wifi_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(wifi_dot, 0, 0);
  lv_obj_set_pos(wifi_dot, 18, 400);

  status_label = lv_label_create(left_panel);
  if (WiFi.status() == WL_CONNECTED) {
    char buf[64];
    snprintf(buf, sizeof(buf), "WiFi Conectado (%s)", WiFi.localIP().toString().c_str());
    lv_label_set_text(status_label, buf);
  } else {
    lv_label_set_text(status_label, "AP: Painel-Config (192.168.4.1)");
  }
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(status_label, colMuted, 0);
  lv_obj_set_pos(status_label, 34, 396);


  // ==========================================
  // PAINEL DIREITO: COTAÇÃO DE MOEDAS (385 x 444)
  // ==========================================

  // Título Dourado "COTAÇÃO DE MOEDAS"
  lv_obj_t *moeda_title = lv_label_create(scr);
  lv_label_set_text(moeda_title, "COTAÇÃO DE MOEDAS");
  lv_obj_set_style_text_font(moeda_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(moeda_title, colHeaderGold, 0);
  lv_obj_set_style_text_letter_space(moeda_title, 2, 0);
  lv_obj_align(moeda_title, LV_ALIGN_TOP_MID, 195, 20);

  // Lista de Pares e Flags Habilitadas
  const char* pairs[6] = {gConfig.currency_1, gConfig.currency_2, gConfig.currency_3, gConfig.currency_4, gConfig.currency_5, gConfig.currency_6};
  bool enabled[6] = {gConfig.curr1_enabled, gConfig.curr2_enabled, gConfig.curr3_enabled, gConfig.curr4_enabled, gConfig.curr5_enabled, gConfig.curr6_enabled};

  int cardY = 56;
  int cardH = 114;
  int cardGap = 16;
  int cardIdx = 0;

  for (int i = 0; i < 6 && cardIdx < 3; i++) {
    if (!enabled[i]) continue;

    // Card Individual de Moeda
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_pos(card, 400, cardY);
    lv_obj_set_size(card, 382, cardH);
    lv_obj_set_style_bg_color(card, colCardRight, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, colBorder, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    moeda_cards[cardIdx] = card;

    // 1. Ícone / Bandeira à Esquerda
    lv_obj_t *iconBox = lv_obj_create(card);
    lv_obj_set_size(iconBox, 44, 38);
    lv_obj_align(iconBox, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_opa(iconBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(iconBox, 0, 0);
    lv_obj_set_style_pad_all(iconBox, 0, 0);
    lv_obj_clear_flag(iconBox, LV_OBJ_FLAG_SCROLLABLE);
    render_currency_icon(iconBox, pairs[i]);
    moeda_icon_boxes[cardIdx] = iconBox;

    // 2. Par de Moedas (ex: "USD/BRL")
    String pStr = String(pairs[i]);
    pStr.replace("-", "/");
    lv_obj_t *pairLbl = lv_label_create(card);
    lv_label_set_text(pairLbl, pStr.c_str());
    lv_obj_set_style_text_font(pairLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(pairLbl, colWhite, 0);
    lv_obj_set_pos(pairLbl, 66, 16);
    moeda_pair_labels[cardIdx] = pairLbl;

    // 3. Subtítulo (ex: "Dólar", "Euro", "Bitcoin")
    lv_obj_t *subLbl = lv_label_create(card);
    lv_label_set_text(subLbl, get_currency_friendly_name(pairs[i]));
    lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(subLbl, colMuted, 0);
    lv_obj_set_pos(subLbl, 66, 52);
    moeda_sub_labels[cardIdx] = subLbl;

    // 4. Preço / Valor (ex: "R$ 4,92", "R$ 171.450")
    lv_obj_t *valLbl = lv_label_create(card);
    lv_label_set_text(valLbl, moedaValues[i].c_str());
    lv_obj_set_style_text_font(valLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(valLbl, colWhite, 0);
    lv_obj_align(valLbl, LV_ALIGN_TOP_RIGHT, -14, 16);
    moeda_value_labels[cardIdx] = valLbl;
    if (cardIdx == 0) dolar_label = valLbl;

    // 5. Variação Percentual (ex: "+0.35% ▲", "-0.12% ▼")
    lv_obj_t *pctLbl = lv_label_create(card);
    lv_label_set_text(pctLbl, moedaPcts[i].c_str());
    lv_obj_set_style_text_font(pctLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pctLbl, moedaPctPos[i] ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);
    lv_obj_align(pctLbl, LV_ALIGN_TOP_RIGHT, -14, 52);
    moeda_pct_labels[cardIdx] = pctLbl;

    cardY += cardH + cardGap;
    cardIdx++;
  }

  // Limpa referências não usadas
  for (int i = cardIdx; i < 6; i++) {
    moeda_cards[i] = nullptr;
    moeda_pair_labels[i] = nullptr;
    moeda_sub_labels[i] = nullptr;
    moeda_value_labels[i] = nullptr;
    moeda_pct_labels[i] = nullptr;
    moeda_icon_boxes[i] = nullptr;
  }
}

// Atualização do Relógio e Data a cada segundo
void update_clock(lv_timer_t *timer) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[16];
    // Formato limpo HH:MM exatamente como na foto de referência
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    static const char *weekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
    static const char *months[] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};
    char dateStr[32];
    snprintf(dateStr, sizeof(dateStr), "%s, %02d %s",
             weekdays[timeinfo.tm_wday], timeinfo.tm_mday, months[timeinfo.tm_mon]);

    if (time_label) lv_label_set_text(time_label, timeStr);
    if (date_label) lv_label_set_text(date_label, dateStr);
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
    // Troca ponto por vírgula no padrão brasileiro
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
    if (enabled[i]) {
      if (list.length()) list += ",";
      list += pairs[i];
    }
  }
  if (list.length() == 0) return;

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
      for (int i = 0; i < 6; i++) {
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
            // Troca ponto por vírgula
            char *pdot = strchr(pctBuf, '.');
            if (pdot) *pdot = ',';

            moedaPcts[i] = String(pctBuf);
            moedaPctPos[i] = isPos;

            if (idx < 3 && moeda_value_labels[idx]) {
              lv_label_set_text(moeda_value_labels[idx], valStr.c_str());
            }
            if (idx < 3 && moeda_pct_labels[idx]) {
              lv_label_set_text(moeda_pct_labels[idx], pctBuf);
              lv_obj_set_style_text_color(moeda_pct_labels[idx], isPos ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);
            }
            if (idx < 3 && moeda_pair_labels[idx]) {
              String p = pairs[i];
              p.replace("-", "/");
              lv_label_set_text(moeda_pair_labels[idx], p.c_str());
            }
            if (idx < 3 && moeda_sub_labels[idx]) {
              lv_label_set_text(moeda_sub_labels[idx], get_currency_friendly_name(pairs[i].c_str()));
            }
            if (idx < 3 && moeda_icon_boxes[idx]) {
              render_currency_icon(moeda_icon_boxes[idx], pairs[i].c_str());
            }

            Serial.println(pairs[i] + ": " + valStr + " (" + String(pctBuf) + ")");
            idx++;
          }
        }
      }
    } else {
      Serial.println("Erro ao parsear JSON awesomeapi");
    }
  } else {
    Serial.printf("Erro HTTP dolar %d list %s\n", httpCode, list.c_str());
  }
  http.end();
}

// Atualização do Clima (Open-Meteo com temperatura, umidade, vento e ícone)
void update_weather(lv_timer_t *timer) {
  if (WiFi.status() != WL_CONNECTED) return;

  char url[220];
  snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&current_weather=true", gConfig.lat, gConfig.lon);
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      float temp = 0;
      int humidity = 0;
      float wind = 0;
      int wcode = 0;

      if (doc["current"].is<JsonObject>()) {
        temp = doc["current"]["temperature_2m"].as<float>();
        humidity = doc["current"]["relative_humidity_2m"].as<int>();
        wind = doc["current"]["wind_speed_10m"].as<float>();
        wcode = doc["current"]["weather_code"].as<int>();
      } else if (doc["current_weather"].is<JsonObject>()) {
        temp = doc["current_weather"]["temperature"].as<float>();
        wcode = doc["current_weather"]["weathercode"].as<int>();
        wind = doc["current_weather"]["windspeed"].as<float>();
        humidity = 64; // fallback
      }

      currentWeatherCode = wcode;

      const char *desc = "Parcialmente Nublado";
      if (wcode == 0) desc = "Ensolarado";
      else if (wcode == 1) desc = "Predom. Ensolarado";
      else if (wcode == 2) desc = "Parcialmente Nublado";
      else if (wcode == 3) desc = "Nublado";
      else if (wcode == 45 || wcode == 48) desc = "Nevoeiro";
      else if (wcode >= 51 && wcode <= 57) desc = "Chuvisco";
      else if (wcode >= 61 && wcode <= 67) desc = "Chuva";
      else if (wcode >= 71 && wcode <= 77) desc = "Neve";
      else if (wcode >= 80 && wcode <= 82) desc = "Pancadas de Chuva";
      else if (wcode >= 95) desc = "Tempestade com Raios";

      char bufTemp[16];
      snprintf(bufTemp, sizeof(bufTemp), "%.0f°C", temp);
      weatherTemp = bufTemp;
      weatherDesc = desc;
      weatherCity = String(gConfig.city);

      char bufHum[32];
      snprintf(bufHum, sizeof(bufHum), "Humidity: %d%%", humidity);
      weatherHumidity = bufHum;

      char bufWind[32];
      snprintf(bufWind, sizeof(bufWind), "Wind: %.0f km/h", wind);
      weatherWind = bufWind;

      if (weather_temp_label) lv_label_set_text(weather_temp_label, weatherTemp.c_str());
      if (weather_desc_label) lv_label_set_text(weather_desc_label, weatherDesc.c_str());
      if (weather_humidity_label) lv_label_set_text(weather_humidity_label, weatherHumidity.c_str());
      if (weather_wind_label) lv_label_set_text(weather_wind_label, weatherWind.c_str());
      if (weather_city_label) {
        String cUpper = weatherCity;
        cUpper.toUpperCase();
        lv_label_set_text(weather_city_label, cUpper.c_str());
      }
      if (weather_icon_box) {
        render_weather_icon(weather_icon_box, currentWeatherCode);
      }

      Serial.printf("Clima Atualizado: %s, %s, %s, %s @ %s\n",
                    weatherTemp.c_str(), weatherDesc.c_str(), weatherHumidity.c_str(), weatherWind.c_str(), weatherCity.c_str());
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
  Serial.println("\n=== SMART DASHBOARD BOOT ===");
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

  // AP de configuracao SEMPRE ligado (http://192.168.4.1)
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  bool apOk = WiFi.softAP("Painel-Config", "12345678");
  if (!apOk) {
    Serial.println("[WiFi] softAP falhou na 1a tentativa, retry...");
    delay(300);
    apOk = WiFi.softAP("Painel-Config", "12345678");
  }
  Serial.printf("[WiFi] AP Painel-Config %s | IP do AP: %s\n", apOk ? "OK" : "FALHOU", WiFi.softAPIP().toString().c_str());

  // Servidor Web + Captive Portal
  webServerInit();

  if (gConfig.wifi_ssid[0] == '\0') {
    Serial.println("[WiFi] Nenhuma rede salva - apenas portal de config");
  } else {
    Serial.printf("[WiFi] Conectando em '%s' ...\n", gConfig.wifi_ssid);
    WiFi.begin(gConfig.wifi_ssid, gConfig.wifi_pass);

    unsigned long start = millis();
    int dot = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      lv_timer_handler();
      webServerLoop();
      if (millis() % 1000 < 20) { Serial.print("."); dot++; if (dot % 60 == 0) Serial.println(); }
      delay(10);
    }
    Serial.println();
    Serial.printf("[WiFi] status=%d (%s)\n", WiFi.status(), WiFi.status() == WL_CONNECTED ? "OK" : "FALHA");
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (wifi_dot) {
      lv_obj_set_style_bg_color(wifi_dot, lv_color_hex(0x00E676), 0);
    }
    if (status_label) {
      char buf[64];
      snprintf(buf, sizeof(buf), "WiFi Conectado (%s)", WiFi.localIP().toString().c_str());
      lv_label_set_text(status_label, buf);
    }
    Serial.println(String("WiFi conectado! IP: ") + WiFi.localIP().toString());
  } else {
    if (wifi_dot) {
      lv_obj_set_style_bg_color(wifi_dot, lv_color_hex(0xFF5252), 0);
    }
    if (status_label) {
      lv_label_set_text(status_label, "AP: Painel-Config (192.168.4.1)");
    }
    Serial.println("Sem WiFi - use o portal http://192.168.4.1/");
  }

  Serial.printf("[Web] Portal config: http://192.168.4.1/  |  STA: http://%s/\n",
                WiFi.localIP().toString().c_str());

  configTime(gConfig.tz_offset * 3600, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  int ntpWait = 0;
  while (!getLocalTime(&timeinfo) && ntpWait < 10) {
    lv_timer_handler();
    webServerLoop();
    delay(500);
    ntpWait++;
  }
  if (getLocalTime(&timeinfo)) Serial.println("Tempo sincronizado!");

  lv_timer_create(update_clock, 1000, NULL);
  lv_timer_create(update_dolar, gConfig.dolar_interval * 1000, NULL);
  lv_timer_create(update_weather, gConfig.weather_interval * 1000, NULL);
  update_dolar(NULL);
  update_weather(NULL);
  update_clock(NULL);

  // OTA auto
  otaInit();
  if (WiFi.status() == WL_CONNECTED) {
    lv_timer_create([](lv_timer_t* t){ otaCheck(true); }, 15000, NULL);
    lv_timer_create([](lv_timer_t* t){ otaCheck(true); }, 6 * 3600 * 1000, NULL);
  }
}

void loop() {
  if (gNeedsRebuild) {
    gNeedsRebuild = false;
    create_ui();
    update_dolar(NULL);
    update_weather(NULL);
    update_clock(NULL);
    if (WiFi.status() == WL_CONNECTED) {
      char buf[64];
      snprintf(buf, sizeof(buf), "WiFi Conectado (%s)", WiFi.localIP().toString().c_str());
      if (status_label) lv_label_set_text(status_label, buf);
    }
  }
  lv_timer_handler();
  webServerLoop();
  otaLoop();
  delay(5);
}
