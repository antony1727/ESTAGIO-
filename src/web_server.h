#pragma once
#include <WebServer.h>
#include <DNSServer.h>

extern WebServer webServer;
extern DNSServer dnsServer;

void webServerInit();
void webServerLoop();
bool isApMode();
