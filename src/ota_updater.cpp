#include "ota_updater.h"
#include "version.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

OtaInfo gOta;

// Update roda em task background para nao travar o webserver/loop
// e nao estourar o Task Watchdog (WDT) que reseta o ESP32 em updates longos
static TaskHandle_t sOtaTaskHandle = NULL;
static volatile bool sOtaRequested = false;
static volatile bool sOtaBusy = false;
static String sOtaUrl = ""; // URL capturada no momento do agendamento (imune a otaCheck limpar gOta.downloadUrl)

static void otaTask(void *param){
  // task dedicada: sem WDT da loopTask, download pode demorar
  for(;;){
    if(sOtaRequested){
      sOtaRequested = false;
      String url = sOtaUrl;
      sOtaUrl = "";
      otaUpdateUrl(url); // usa a URL capturada no agendamento
      sOtaBusy = false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

static String normalizeVersion(String v){
  v.replace("v",""); v.replace("V",""); v.trim();
  return v;
}

static int versionToCode(String v){
  v = normalizeVersion(v);
  int maj=0, min=0, pat=0;
  sscanf(v.c_str(), "%d.%d.%d", &maj, &min, &pat);
  return maj*100 + min*10 + pat;
}

String otaGetCurrentVersion(){ return String(FIRMWARE_VERSION); }

void otaInit(){
  gOta.current = String(FIRMWARE_VERSION);
  gOta.state = OTA_IDLE;
  if(sOtaTaskHandle == NULL){
    // stack generosa: HTTPClient + WiFiClient + buffer de download estouram 8KB facilmente
    xTaskCreatePinnedToCore(otaTask, "ota_task", 32768, NULL, 1, &sOtaTaskHandle, 0);
  }
  Serial.printf("[OTA] versao atual %s (%d)\n", FIRMWARE_VERSION, FIRMWARE_VERSION_CODE);
}

void otaRequestUpdate(){
  if(!sOtaBusy && gOta.downloadUrl.length()>0){
    sOtaUrl = gOta.downloadUrl; // captura a URL AGORA; otaCheck nao consegue mais limpar
    sOtaBusy = true;
    sOtaRequested = true; // task em background executa o update
  }
}

// Fallback sem API: segue o redirect de /releases/latest e extrai a tag
// Nao sofre rate limit de 60 req/h da api.github.com
static bool checkViaRedirect(String &tagOut){
  WiFiClientSecure client;
  client.setInsecure(); // Necessario para conexao HTTPS no GitHub
  HTTPClient http;
  http.setTimeout(15000);
  http.addHeader("User-Agent", "ESP32-OTA");
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  String url = String("https://github.com/") + GITHUB_REPO + "/releases/latest";
  http.begin(client, url);
  int code = http.GET();
  if(code == 301 || code == 302 || code == 303 || code == 307 || code == 308){
    String loc = http.getLocation();
    http.end();
    int pos = loc.lastIndexOf("/tag/");
    if(pos >= 0){
      tagOut = loc.substring(pos + 5);
      Serial.printf("[OTA] fallback redirect -> %s\n", tagOut.c_str());
      return true;
    }
    gOta.error = "fallback: redirect sem /tag/";
    Serial.println("[OTA] " + gOta.error + " loc=" + loc);
    return false;
  }
  if(code == 200){
    // pagina HTML com meta refresh ou link canonical
    String body = http.getString();
    http.end();
    int i = body.indexOf("/releases/tag/");
    if(i >= 0){
      int start = i + strlen("/releases/tag/");
      int endQ = body.indexOf('"', start);
      int endS = body.indexOf('\'', start);
      int end = (endQ >= 0 && (endS < 0 || endQ < endS)) ? endQ : endS;
      if(end > start){
        String path = body.substring(start, end);
        tagOut = path.substring(path.lastIndexOf('/') + 1);
        Serial.printf("[OTA] fallback html -> %s\n", tagOut.c_str());
        return true;
      }
    }
    gOta.error = "fallback: html sem tag";
    return false;
  }
  http.end();
  gOta.error = "fallback HTTP " + String(code);
  Serial.println("[OTA] " + gOta.error);
  return false;
}

bool otaCheck(bool showLog){
  if(gOta.state == OTA_UPDATING) return false; // nao interfere durante update
  if(WiFi.status()!=WL_CONNECTED){
    gOta.error = "WiFi desconectado";
    gOta.state = OTA_FAILED;
    return false;
  }
  gOta.state = OTA_CHECKING;
  gOta.error = "";
  gOta.downloadUrl = "";
  if(showLog) Serial.println("[OTA] verificando release mais recente no GitHub...");

  String tag = "";
  bool gotTag = false;

  // 1) Tentativa via API oficial com WiFiClientSecure
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(15000);
  http.addHeader("User-Agent", "ESP32-OTA");
  http.addHeader("Accept", "application/vnd.github+json");
  http.begin(client, GITHUB_API_LATEST);
  int code = http.GET();
  if(code == 200){
    String payload = http.getString();
    http.end();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if(err){
      gOta.error = "JSON erro";
      Serial.println("[OTA] JSON erro");
    } else {
      tag = doc["tag_name"] | "";
      if(tag.length()){
        gotTag = true;
        JsonArray assets = doc["assets"];
        for(JsonObject a : assets){
          String name = a["name"] | "";
          if(name.endsWith(".bin")){
            gOta.downloadUrl = a["browser_download_url"] | "";
            break;
          }
        }
      } else {
        gOta.error = "sem tag_name";
      }
    }
  } else {
    String body = http.getString();
    http.end();
    body.replace("\n", " ");
    if(body.length() > 180) body = body.substring(0, 180);
    Serial.printf("[OTA] API HTTP %d corpo: %s\n", code, body.c_str());
    gOta.error = "API HTTP " + String(code) + ", tentando fallback";
  }

  // 2) Fallback via redirect (sem rate limit)
  if(!gotTag){
    if(checkViaRedirect(tag)){
      gotTag = true;
      gOta.error = ""; // fallback funcionou, limpa erro da API
    }
  }

  if(!gotTag){
    gOta.state = OTA_FAILED;
    if(gOta.error.length()==0) gOta.error = "nenhum release encontrado";
    Serial.printf("[OTA] falha: %s\n", gOta.error.c_str());
    return false;
  }

  gOta.latest = normalizeVersion(tag);

  // se a API nao deu a URL do asset, monta a padrao do Action
  if(gOta.downloadUrl.length()==0){
    gOta.downloadUrl = String("https://github.com/") + GITHUB_REPO +
                       "/releases/download/" + tag + "/firmware-" + tag + ".bin";
  }

  int latestCode = versionToCode(tag);
  int curCode = FIRMWARE_VERSION_CODE;
  Serial.printf("[OTA] atual=%s (%d) latest=%s (%d)\n", gOta.current.c_str(), curCode, gOta.latest.c_str(), latestCode);
  Serial.printf("[OTA] url=%s\n", gOta.downloadUrl.c_str());

  if(latestCode > curCode && gOta.downloadUrl.length()>0){
    gOta.error = "Atualizacao disponivel: " + tag;
    if(showLog) Serial.println("[OTA] " + gOta.error + " -> disparando atualizacao automatica!");
    otaRequestUpdate(); // Dispara imediatamente o download e gravacao em background
    return true;
  } else {
    gOta.state = OTA_NO_UPDATE;
    gOta.error = "Ja esta na ultima versao";
    if(showLog) Serial.println("[OTA] ja atualizado");
    return false;
  }
}

bool otaUpdate(){
  // chamada direta (ex: manual): usa a URL do ultimo check
  return otaUpdateUrl(gOta.downloadUrl);
}

bool otaUpdateUrl(String url){
  if(url.length()==0){
    gOta.error = "sem URL, faca Verificar antes";
    gOta.state = OTA_FAILED;
    Serial.println("[OTA] " + gOta.error);
    return false;
  }
  if(WiFi.status()!=WL_CONNECTED){
    gOta.error = "WiFi desconectado";
    gOta.state = OTA_FAILED;
    return false;
  }
  gOta.state = OTA_UPDATING;
  gOta.progress = 0;
  Serial.println("[OTA] iniciando download " + url);

  WiFiClientSecure client;
  client.setInsecure(); // Permite SSL/TLS sem rejeitar certificado do GitHub / AWS S3
  HTTPClient http;
  http.setTimeout(30000);
  http.addHeader("User-Agent", "ESP32-OTA");
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.begin(client, url);
  int code = http.GET();

  // fallback de nome de arquivo: firmware-latest.bin
  if(code == 404){
    http.end();
    String alt = String("https://github.com/") + GITHUB_REPO +
                 "/releases/download/v" + gOta.latest + "/firmware-latest.bin";
    Serial.println("[OTA] 404, tentando " + alt);
    http.begin(client, alt);
    http.addHeader("User-Agent", "ESP32-OTA");
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    code = http.GET();
  }

  if(code != 200){
    gOta.error = "download HTTP " + String(code);
    gOta.state = OTA_FAILED;
    Serial.printf("[OTA] download falha %d\n", code);
    http.end();
    return false;
  }
  int len = http.getSize();
  Serial.printf("[OTA] tamanho %d bytes\n", len);
  if(!Update.begin(len > 0 ? len : UPDATE_SIZE_UNKNOWN)){
    gOta.error = Update.errorString();
    gOta.state = OTA_FAILED;
    http.end();
    return false;
  }
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buff[4096];
  int written = 0;
  unsigned long last = millis();
  while(http.connected() && (len > 0 || len == -1)){
    esp_task_wdt_reset(); // alimenta WDT durante download longo
    size_t size = stream->available();
    if(size){
      int c = stream->readBytes(buff, (size > sizeof(buff)) ? sizeof(buff) : size);
      if(Update.write(buff, c) != c){
        gOta.error = Update.errorString();
        gOta.state = OTA_FAILED;
        Update.abort();
        http.end();
        return false;
      }
      written += c;
      if(len > 0){
        gOta.progress = (written * 100) / len;
      }
      if(millis() - last > 500){
        last = millis();
        Serial.printf("[OTA] progresso %d%% (%d/%d)\n", gOta.progress, written, len);
      }
    }
    if(len > 0 && written >= len) break;
    delay(1);
  }
  http.end();

  if(Update.end(true)){
    if(Update.isFinished()){
      gOta.state = OTA_SUCCESS;
      gOta.progress = 100;
      Serial.println("[OTA] Sucesso! Reiniciando...");
      delay(800);
      ESP.restart();
      return true;
    } else {
      gOta.error = "Update nao finalizado";
      gOta.state = OTA_FAILED;
      return false;
    }
  } else {
    gOta.error = Update.errorString();
    gOta.state = OTA_FAILED;
    Serial.printf("[OTA] Erro gravacao: %s\n", gOta.error.c_str());
    return false;
  }
}

void otaLoop(){
}
