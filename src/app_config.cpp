#include "app_config.h"

AppConfig gConfig;
static Preferences prefs;

void loadConfig() {
  prefs.begin("painel", true);
  bool firstBoot = !prefs.isKey("ssid");
  if(firstBoot){
    Serial.println("[Config] primeira inicializacao detectada (NVS vazio) -> modo portal WiFiManager");
  }
  String s;
  s = prefs.getString("ssid", gConfig.wifi_ssid);
  s.toCharArray(gConfig.wifi_ssid, sizeof(gConfig.wifi_ssid));
  s = prefs.getString("pass", gConfig.wifi_pass);
  s.toCharArray(gConfig.wifi_pass, sizeof(gConfig.wifi_pass));
  if(firstBoot){
    // em primeiro boot apos erase, força AP em vez de usar default ROBOBUILDERS
    // deixa SSID vazio para main.cpp cair no portal
    gConfig.wifi_ssid[0] = '\0';
    gConfig.wifi_pass[0] = '\0';
    Serial.println("[WiFiManager] Nenhuma rede salva -> ira iniciar AP Painel-Config");
  }

  s = prefs.getString("c1", gConfig.currency_1);
  s.toCharArray(gConfig.currency_1, sizeof(gConfig.currency_1));
  s = prefs.getString("c2", gConfig.currency_2);
  s.toCharArray(gConfig.currency_2, sizeof(gConfig.currency_2));
  s = prefs.getString("c3", gConfig.currency_3);
  s.toCharArray(gConfig.currency_3, sizeof(gConfig.currency_3));
  s = prefs.getString("c4", gConfig.currency_4);
  s.toCharArray(gConfig.currency_4, sizeof(gConfig.currency_4));
  s = prefs.getString("c5", gConfig.currency_5);
  s.toCharArray(gConfig.currency_5, sizeof(gConfig.currency_5));
  s = prefs.getString("c6", gConfig.currency_6);
  s.toCharArray(gConfig.currency_6, sizeof(gConfig.currency_6));

  gConfig.curr1_enabled = prefs.getBool("c1en", gConfig.curr1_enabled);
  gConfig.curr2_enabled = prefs.getBool("c2en", gConfig.curr2_enabled);
  gConfig.curr3_enabled = prefs.getBool("c3en", gConfig.curr3_enabled);
  gConfig.curr4_enabled = prefs.getBool("c4en", gConfig.curr4_enabled);
  gConfig.curr5_enabled = prefs.getBool("c5en", gConfig.curr5_enabled);
  gConfig.curr6_enabled = prefs.getBool("c6en", gConfig.curr6_enabled);

  s = prefs.getString("city", gConfig.city);
  s.toCharArray(gConfig.city, sizeof(gConfig.city));

  gConfig.lat = prefs.getFloat("lat", gConfig.lat);
  gConfig.lon = prefs.getFloat("lon", gConfig.lon);
  gConfig.brightness = prefs.getUChar("bright", gConfig.brightness);
  gConfig.tz_offset = prefs.getChar("tz", gConfig.tz_offset);
  gConfig.dolar_interval = prefs.getUShort("dint", gConfig.dolar_interval);
  gConfig.weather_interval = prefs.getUShort("wint", gConfig.weather_interval);
  gConfig.display_light = prefs.getBool("dlight", gConfig.display_light);
  prefs.end();
  Serial.println("[Config] carregado");
}

void saveConfig() {
  prefs.begin("painel", false);
  prefs.putString("ssid", gConfig.wifi_ssid);
  prefs.putString("pass", gConfig.wifi_pass);
  prefs.putString("c1", gConfig.currency_1);
  prefs.putString("c2", gConfig.currency_2);
  prefs.putString("c3", gConfig.currency_3);
  prefs.putString("c4", gConfig.currency_4);
  prefs.putString("c5", gConfig.currency_5);
  prefs.putString("c6", gConfig.currency_6);
  prefs.putBool("c1en", gConfig.curr1_enabled);
  prefs.putBool("c2en", gConfig.curr2_enabled);
  prefs.putBool("c3en", gConfig.curr3_enabled);
  prefs.putBool("c4en", gConfig.curr4_enabled);
  prefs.putBool("c5en", gConfig.curr5_enabled);
  prefs.putBool("c6en", gConfig.curr6_enabled);
  prefs.putString("city", gConfig.city);
  prefs.putFloat("lat", gConfig.lat);
  prefs.putFloat("lon", gConfig.lon);
  prefs.putUChar("bright", gConfig.brightness);
  prefs.putChar("tz", gConfig.tz_offset);
  prefs.putUShort("dint", gConfig.dolar_interval);
  prefs.putUShort("wint", gConfig.weather_interval);
  prefs.putBool("dlight", gConfig.display_light);
  prefs.end();
  Serial.println("[Config] salvo");
}

void resetConfig() {
  prefs.begin("painel", false);
  prefs.clear();
  prefs.end();
}
