#include <Servo.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Nano Servo Shutter adaptation. Servo signal: D9.
// EXPOSE ms is command-to-close timing, NOT calibrated optical exposure.
static const byte SERVO_PIN = 9;
static const unsigned long BAUD = 115200;
Servo shutter;
char inputLine[120];
byte inputLen = 0;
int openAngle = 68, closedAngle = 0, reliefAngle = 4;
unsigned long travelMs = 150;
bool isOpen = false, exposureActive = false;
unsigned long closeAt = 0;

bool getString(const char *s, const char *key, char *out, size_t n) {
  char k[24]; snprintf(k, sizeof(k), "\"%s\"", key);
  const char *p = strstr(s, k); if (!p || !(p = strchr(p, ':'))) return false;
  while (*++p && isspace((unsigned char)*p)) {}
  if (*p != '"') return false; ++p;
  size_t i=0; while (*p && *p!='"' && i+1<n) out[i++]=*p++;
  out[i]='\0'; return *p=='"';
}
bool getNumber(const char *s, const char *key, long *v) {
  char k[24]; snprintf(k, sizeof(k), "\"%s\"", key);
  const char *p=strstr(s,k); if(!p || !(p=strchr(p,':'))) return false;
  ++p; while(*p && isspace((unsigned char)*p)) ++p;
  char *end; long x=strtol(p,&end,10); if(end==p) return false; *v=x; return true;
}
void status() {
  Serial.print("{\"ok\":true,\"open\":"); Serial.print(isOpen?"true":"false");
  Serial.print(",\"exposureActive\":"); Serial.print(exposureActive?"true":"false");
  Serial.print(",\"openAngle\":"); Serial.print(openAngle);
  Serial.print(",\"closedAngle\":"); Serial.print(closedAngle);
  Serial.print(",\"reliefAngle\":"); Serial.print(reliefAngle);
  Serial.print(",\"travelMs\":"); Serial.print(travelMs); Serial.println("}");
}
void closeShutter(const char *eventName) {
  exposureActive=false; shutter.write(closedAngle); isOpen=false;
  Serial.print("{\"ok\":true,\"event\":\""); Serial.print(eventName); Serial.println("\"}");
}
void handle(char *s) {
  char cmd[16];
  if(!getString(s,"cmd",cmd,sizeof(cmd))) { Serial.println("{\"ok\":false,\"error\":\"missing cmd\"}"); return; }
  if(!strcmp(cmd,"OPEN")) {
    exposureActive=false; shutter.write(openAngle); isOpen=true;
    Serial.println("{\"ok\":true,\"event\":\"opened\"}");
  } else if(!strcmp(cmd,"CLOSE")) closeShutter("closed");
  else if(!strcmp(cmd,"CANCEL")) closeShutter("cancelled_closed");
  else if(!strcmp(cmd,"STATUS")) status();
  else if(!strcmp(cmd,"EXPOSE")) {
    long ms;
    if(!getNumber(s,"ms",&ms)||ms<1||ms>3600000L) { Serial.println("{\"ok\":false,\"error\":\"ms must be 1..3600000\"}"); return; }
    shutter.write(openAngle); isOpen=true; exposureActive=true; closeAt=millis()+(unsigned long)ms;
    Serial.println("{\"ok\":true,\"event\":\"exposure_started\"}");
  } else if(!strcmp(cmd,"SET")) {
    long v;
    if(getNumber(s,"open",&v)) openAngle=constrain((int)v,0,180);
    if(getNumber(s,"closed",&v)) closedAngle=constrain((int)v,0,180);
    if(getNumber(s,"relief",&v)) reliefAngle=constrain((int)v,0,180);
    if(getNumber(s,"travelMs",&v)) travelMs=constrain(v,50L,2000L);
    shutter.write(isOpen?openAngle:closedAngle);
    Serial.println("{\"ok\":true,\"event\":\"settings_updated\"}");
  } else Serial.println("{\"ok\":false,\"error\":\"unknown cmd\"}");
}
void setup() {
  Serial.begin(BAUD); shutter.attach(SERVO_PIN); shutter.write(closedAngle);
  isOpen=false; delay(300);
  Serial.println("{\"ok\":true,\"event\":\"ready\",\"shutter\":\"closed\"}");
}
void loop() {
  while(Serial.available()) {
    char c=(char)Serial.read(); if(c=='\r') continue;
    if(c=='\n') { inputLine[inputLen]='\0'; if(inputLen) handle(inputLine); inputLen=0; }
    else if(inputLen<sizeof(inputLine)-1) inputLine[inputLen++]=c;
    else { inputLen=0; Serial.println("{\"ok\":false,\"error\":\"line too long\"}"); }
  }
  if(exposureActive && (long)(millis()-closeAt)>=0) closeShutter("exposure_closed");
}
