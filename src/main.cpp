#include <Arduino.h>
#include <Wire.h>
#include <WiFiS3.h>
#include <Servo.h>
#include "Arduino_Modulino.h"
#include "Arduino_LED_Matrix.h"
#include "config.h"

// Field fallback network, used when no known network is in range. Both must be
// defined in config.h (git-ignored) so that no credential of any kind lives in
// this repository. WPA2 requires a passphrase of at least 8 characters.
#ifndef AP_SSID
#error "Define AP_SSID in src/config.h"
#endif
#ifndef AP_PASS
#error "Define AP_PASS in src/config.h"
#endif

// --- WIFI CREDENTIALS ---
// Known networks, tried in order at boot. Credentials live in config.h so they stay
// out of git; define WIFI_SSID2/WIFI_SSID3 there to add more.
struct WifiCred { const char* ssid; const char* pass; };
const WifiCred knownNets[] = {
  { WIFI_SSID, WIFI_PASS },
#ifdef WIFI_SSID2
  { WIFI_SSID2, WIFI_PASS2 },
#endif
#ifdef WIFI_SSID3
  { WIFI_SSID3, WIFI_PASS3 },
#endif
};
const int NUM_NETS = sizeof(knownNets) / sizeof(knownNets[0]);

WiFiServer server(80);
ArduinoLEDMatrix matrix;
ModulinoKnob knob;
Servo myservo; 

// 1. MANUAL FONT DEFINITION (8 rows x 5 columns)
// Each byte is one row; only the low 5 bits are used, MSB = leftmost column.
const uint8_t font[41][8] = {
  {0x0E,0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // 0
  {0x04,0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}, // 1
  {0x0E,0x11,0x01,0x02,0x04,0x08,0x10,0x1F}, // 2
  {0x1F,0x02,0x04,0x02,0x01,0x11,0x11,0x0E}, // 3
  {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02,0x02}, // 4
  {0x1F,0x10,0x1E,0x01,0x01,0x11,0x11,0x0E}, // 5
  {0x06,0x08,0x10,0x1E,0x11,0x11,0x11,0x0E}, // 6
  {0x1F,0x01,0x02,0x04,0x08,0x08,0x08,0x08}, // 7
  {0x0E,0x11,0x11,0x0E,0x11,0x11,0x11,0x0E}, // 8
  {0x0E,0x11,0x11,0x0F,0x01,0x02,0x04,0x08}, // 9
  {0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, // .   (10)
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' ' (11)
  {0x04,0x0A,0x11,0x11,0x1F,0x11,0x11,0x11}, // A   (12)
  {0x1E,0x11,0x11,0x1E,0x11,0x11,0x11,0x1E}, // B
  {0x0E,0x11,0x10,0x10,0x10,0x10,0x11,0x0E}, // C
  {0x1C,0x12,0x11,0x11,0x11,0x11,0x12,0x1C}, // D
  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10,0x1F}, // E
  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10,0x10}, // F
  {0x0E,0x11,0x10,0x10,0x13,0x11,0x11,0x0E}, // G
  {0x11,0x11,0x11,0x1F,0x11,0x11,0x11,0x11}, // H
  {0x0E,0x04,0x04,0x04,0x04,0x04,0x04,0x0E}, // I
  {0x07,0x02,0x02,0x02,0x02,0x12,0x12,0x0C}, // J
  {0x11,0x12,0x14,0x18,0x18,0x14,0x12,0x11}, // K
  {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // L
  {0x11,0x1B,0x15,0x15,0x11,0x11,0x11,0x11}, // M
  {0x11,0x19,0x19,0x15,0x15,0x13,0x13,0x11}, // N
  {0x0E,0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // O
  {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10,0x10}, // P
  {0x0E,0x11,0x11,0x11,0x11,0x15,0x12,0x0D}, // Q
  {0x1E,0x11,0x11,0x1E,0x14,0x12,0x12,0x11}, // R
  {0x0E,0x11,0x10,0x0E,0x01,0x01,0x11,0x0E}, // S
  {0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0x04}, // T
  {0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // U
  {0x11,0x11,0x11,0x11,0x11,0x0A,0x0A,0x04}, // V
  {0x11,0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, // W
  {0x11,0x11,0x0A,0x04,0x04,0x0A,0x11,0x11}, // X
  {0x11,0x11,0x0A,0x04,0x04,0x04,0x04,0x04}, // Y
  {0x1F,0x01,0x02,0x04,0x04,0x08,0x10,0x1F}, // Z   (37)
  {0x00,0x00,0x00,0x1F,0x00,0x00,0x00,0x00}, // -   (38)
  {0x00,0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}, // :   (39)
  {0x19,0x19,0x02,0x04,0x04,0x08,0x13,0x13}  // %   (40)
};

// Map a character onto the font table. Anything unrecognised renders as a blank.
int glyphIndex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c == '.') return 10;
  if (c >= 'A' && c <= 'Z') return 12 + (c - 'A');
  if (c >= 'a' && c <= 'z') return 12 + (c - 'a');
  if (c == '-') return 38;
  if (c == ':') return 39;
  if (c == '%') return 40;
  return 11; // space
}

// --- SYSTEM CONSTANTS ---
// Maximum deviation from the neutral 90-degree position.
// The servo will oscillate between (90 - MAX_DEVIATION) and (90 + MAX_DEVIATION).
const int MAX_DEVIATION = 35; 

// 2. SLIDING WINDOW SCROLLER VARIABLES
// 256 columns at 6 per character holds ~40 characters, enough for an IP plus a
// status line. buildMegaBuffer() refuses to write past the end.
#define MEGA_COLS 256
uint8_t megaBuffer[8][MEGA_COLS]; // Large buffer for the full string
int megaWidth = 0;
int windowIndex = 0;
unsigned long lastScrollTime = 0;

// --- DISPLAY STATE ---
bool matrixOn = true;             // /led?v=0 blanks the panel for servo-less bench work
String matrixText = "";           // what is currently loaded in the scroller
unsigned long bannerUntil = 0;    // a transient message outranks the status line until this time

void buildMegaBuffer(String msg) {
  megaWidth = 0;
  memset(megaBuffer, 0, sizeof(megaBuffer));
  String fullMsg = "  " + msg + "  "; // Reduced padding from 4 spaces to 2
  for (int i = 0; i < fullMsg.length(); i++) {
    // Each glyph costs 5 columns plus 1 of spacing. Stop rather than overrun.
    if (megaWidth + 6 > MEGA_COLS) break;
    int fontIdx = glyphIndex(fullMsg[i]);
    
    for (int col = 0; col < 5; col++) {
      for (int row = 0; row < 8; row++) {
        if (font[fontIdx][row] & (1 << (4 - col))) megaBuffer[row][megaWidth] = 255;
      }
      megaWidth++;
    }
    megaWidth++; // Character spacing
  }
  if (megaWidth < 1) megaWidth = 1; // the renderer takes a modulo of this
}

String webLog = ""; 
unsigned long lastWiFiCheck = 0;
bool serverStarted = false;
String lastIP = "";

// Oscillation State Variables
float currentPhase = 0.0;
unsigned long lastLoopTime = 0;

void logMessage(String msg) {
  webLog = "[" + String(millis()/1000) + "s] " + msg + "<br>" + webLog;
  if (webLog.length() > 3000) webLog = webLog.substring(0, 3000);
  Serial.println(msg); 
}

// --- CONTROL STATE ---
// The speed setpoint can come from two places: the physical Modulino knob, or an
// HTTP command. Rather than a mode the user has to remember, whichever input
// spoke most recently owns the setpoint -- so neither one can lock the other out.
enum ControlSource { SRC_KNOB, SRC_WIFI };
ControlSource controlSource = SRC_KNOB;
bool  running     = true;  // toggled by /on and /off
float wifiPct     = 0.0;   // speed last commanded over WiFi, 0-100
int   lastKnobPos = 0;     // previous knob reading, to detect a physical turn
int   activePct   = 0;     // speed currently in force, whichever source owns it
int   servoAngle  = 90;    // published to /status

// --- NETWORK STATE ---
// At home the board joins WIFI_SSID as a normal client. In the field that network
// does not exist, so after JOIN_TIMEOUT_MS it gives up and hosts its own access
// point instead. Either way the result is an HTTP server on a reachable address.
enum NetMode { NET_JOINING, NET_STATION, NET_AP };
NetMode netMode = NET_JOINING;
unsigned long joinDeadline = 0;
const unsigned long JOIN_TIMEOUT_MS = 15000;
String netLabel = "JOINING";
int  netIndex   = 0;      // which knownNets entry is being tried
bool joinSingle = false;  // true when the user picked one network explicitly
// Network switches are deferred out of the HTTP handler: tearing the connection down
// mid-response makes the request look like it failed to the browser that sent it.
int  pendingJoin = -1;    // >= 0 means "join knownNets[pendingJoin]"
bool pendingAP   = false;

// AP mode never reports WL_CONNECTED -- it reports WL_AP_LISTENING with no client
// attached and WL_AP_CONNECTED with one -- so readiness cannot be a status test alone.
bool netReady() {
  if (netMode == NET_STATION) return WiFi.status() == WL_CONNECTED;
  return netMode == NET_AP;
}

// The default matrix content: who is in charge, how fast, and whether it is moving.
String statusString() {
  if (!netReady()) return "SEARCHING";
  String s = (controlSource == SRC_WIFI) ? "WIFI " : "KNOB ";
  s += String(activePct);
  s += "% ";
  s += (running && activePct > 0) ? "RUN" : "STOP";
  return s;
}

// Rebuild the scroller only when the text actually changes. The scroll position is
// deliberately NOT reset here: a status line ticking 49% -> 50% while you turn the
// knob would otherwise jump back to the start on every increment.
void showText(const String &t) {
  if (t == matrixText) return;
  matrixText = t;
  buildMegaBuffer(t);
}

// A banner is a transient message that outranks the status line for holdMs.
// Unlike showText it restarts the scroll, so the message can be read from its start.
void showBanner(const String &t, unsigned long holdMs) {
  matrixText = t;
  buildMegaBuffer(t);
  windowIndex = 0;
  bannerUntil = millis() + holdMs;
}

// Give up on WIFI_SSID and host our own network instead.
void startJoin(int idx, bool single);  // defined below; startAccessPoint retries via it

void startAccessPoint(bool afterFailure) {
  if (afterFailure) logMessage("No known network in range. Starting own network...");
  else              logMessage("Starting own network on request...");
  if (serverStarted) { server.end(); serverStarted = false; }
  // Pin a predictable address so the URL is known before you can read the panel.
  WiFi.config(IPAddress(192, 168, 4, 1));
  if (WiFi.beginAP(AP_SSID, AP_PASS) == WL_AP_LISTENING) {
    netMode = NET_AP;
    // Report whatever address the modem actually took, not what we asked for.
    lastIP = WiFi.localIP().toString();
    netLabel = "AP " + String(AP_SSID) + " " + lastIP;
    logMessage("AP up. Join '" + String(AP_SSID) + "' then browse to " + lastIP);
    showBanner(String(afterFailure ? "NO WIFI - AP " : "AP ") + String(AP_SSID) + " - " + lastIP, 30000);
  } else {
    // Fall back to retrying the home network rather than ending up with no server.
    logMessage("AP failed to start. Retrying station mode.");
    showBanner("AP FAILED", 4000);
    startJoin(0, false);
  }
}

// Begin joining knownNets[idx]. When 'single' the user picked this network explicitly,
// so a failure drops straight to AP rather than walking the rest of the list.
void startJoin(int idx, bool single) {
  if (idx < 0 || idx >= NUM_NETS) idx = 0;
  if (serverStarted) { server.end(); serverStarted = false; }
  netIndex   = idx;
  joinSingle = single;
  netMode    = NET_JOINING;
  lastIP     = "";
  joinDeadline = millis() + JOIN_TIMEOUT_MS;
  netLabel = "JOINING " + String(knownNets[idx].ssid);
  logMessage("Trying '" + String(knownNets[idx].ssid) + "'...");
  showBanner("JOINING " + String(knownNets[idx].ssid), 4000);
  WiFi.begin(knownNets[idx].ssid, knownNets[idx].pass);
}

// Apply a command path such as "/on" or "/speed?v=50".
// Returns false if the path is not a command, in which case it is treated as a page request.
bool applyCommand(const String &path) {
  if (path.startsWith("/speed")) {
    int q = path.indexOf("?v=");
    if (q < 0) return false;
    int v = path.substring(q + 3).toInt();
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    wifiPct = (float)v;
    controlSource = SRC_WIFI;
    // Asking for a non-zero speed implies you want it moving; asking for 0 stops
    // it anyway via the frequency deadband, so leave 'running' alone in that case.
    if (v > 0) running = true;
    logMessage("CMD speed = " + String(v) + "%");
    showBanner("SPEED " + String(v), 3000);
    return true;
  }
  if (path.startsWith("/led")) {
    int q = path.indexOf("?v=");
    if (q < 0) return false;
    matrixOn = (path.substring(q + 3).toInt() != 0);
    logMessage(matrixOn ? "CMD matrix on" : "CMD matrix off");
    if (matrixOn) showBanner("MATRIX ON", 2500);
    return true;
  }
  if (path.startsWith("/center")) {
    controlSource = SRC_WIFI;
    running = false;
    currentPhase = 0.0;
    // Move immediately rather than waiting for the next loop iteration.
    servoAngle = 90;
    myservo.write(90);
    logMessage("CMD center");
    showBanner("CENTER", 2500);
    return true;
  }
  if (path.startsWith("/off")) {
    controlSource = SRC_WIFI;
    running = false;
    logMessage("CMD off");
    showBanner("OFF", 2500);
    return true;
  }
  if (path.startsWith("/on")) {
    controlSource = SRC_WIFI;
    running = true;
    logMessage("CMD on");
    showBanner("ON", 2500);
    return true;
  }
  if (path.startsWith("/join")) {
    int q = path.indexOf("?n=");
    if (q < 0) return false;
    int n = path.substring(q + 3).toInt();
    if (n < 0 || n >= NUM_NETS) return false;
    pendingJoin = n;  // acted on in loop(), once this response has been sent
    logMessage("CMD join '" + String(knownNets[n].ssid) + "'");
    return true;
  }
  if (path.startsWith("/ap")) {
    pendingAP = true;
    logMessage("CMD force AP");
    return true;
  }
  if (path.startsWith("/ip")) {
    logMessage("CMD show ip");
    if (lastIP != "") showBanner(lastIP, 20000);
    return true;
  }
  return false;
}

// Machine-readable state for the dashboard's 1 Hz poll and for scripts.
// Fields are joined with |~| and split client-side, which avoids having to
// JSON-escape the HTML already sitting in webLog.
void sendStatus(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type: text/plain; charset=utf-8");
  client.println("Connection: close");
  client.println();
  client.print(activePct);                                    client.print("|~|");
  client.print(servoAngle);                                   client.print("|~|");
  client.print(running ? 1 : 0);                              client.print("|~|");
  client.print(controlSource == SRC_WIFI ? "WIFI" : "KNOB");   client.print("|~|");
  client.print(matrixOn ? 1 : 0);                             client.print("|~|");
  client.print(matrixText);                                   client.print("|~|");
  client.print(netLabel);                                     client.print("|~|");
  // Which dropdown entry is live, so the page can mark and preselect it.
  client.print(netMode == NET_AP ? String("ap") : String(netIndex)); client.print("|~|");
  client.print(webLog);
}

void sendDashboard(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type: text/html; charset=utf-8");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE HTML><html><head>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<style>");
  client.println("body { font-family: sans-serif; text-align: center; background-color: #f0f2f5; padding: 10px; color: #333; }");
  client.println(".card { background: white; padding: 20px; border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.1); display: block; margin: auto; max-width: 800px; }");
  client.println("h1 { color: #1a73e8; margin: 0 0 4px; }");
  client.println(".net { color:#5f6368; margin-bottom:18px; }");
  client.println("fieldset.sec { border:2px solid #dadce0; border-radius:10px; padding:14px 18px 20px; margin:18px 0; }");
  client.println("fieldset.sec legend { padding:0 10px; font-weight:bold; letter-spacing:2px; color:#5f6368; font-size:0.85em; }");
  client.println(".data-row { display:flex; justify-content:space-around; flex-wrap:wrap; font-size:1.1em; margin:6px 0 16px; font-weight:bold; }");
  client.println("button { font-size:1.05em; font-weight:bold; padding:12px 30px; margin:5px; border:0; border-radius:8px; cursor:pointer; background:#dadce0; color:#5f6368; }");
  client.println("button.on-act { background:#34a853; color:#fff; }");
  client.println("button.off-act { background:#ea4335; color:#fff; }");
  client.println("button.led-act { background:#1a73e8; color:#fff; }");
  client.println("button.action { background:#5f6368; color:#fff; }");
  client.println("input[type=range] { width:92%; max-width:600px; margin-top:8px; }");
  client.println("select { font-size:1.05em; padding:11px 14px; border-radius:8px; border:2px solid #dadce0; margin:5px; background:#fff; color:#333; }");
  client.println(".hint { color:#5f6368; margin-top:12px; min-height:1.2em; line-height:1.4; }");
  client.println(".panel { background:#111; color:#ff5f1f; font-family:'Courier New',monospace; font-size:1.2em; font-weight:bold; letter-spacing:3px; padding:12px; border-radius:8px; margin-top:14px; border:2px solid #444; }");
  client.println(".log { text-align:left; background:#000; padding:20px; border-radius:8px; height:300px; overflow-y:auto; font-family:'Courier New',monospace; color:#33ff33; margin-top:20px; font-size:14px; border:2px solid #444; }");
  client.println("</style></head><body>");
  client.println("<div class='card'>");
  client.println("<h1>R4 System Control</h1>");
  client.println("<div class='net'>Network: <strong id='net'>-</strong></div>");

  // --- SERVO ---------------------------------------------------------------
  client.println("<fieldset class='sec'><legend>SERVO</legend>");
  client.println("<div class='data-row'>");
  client.println("<div>Speed: <span id='pct' style='color:#e91e63;'>-</span></div>");
  client.println("<div>Angle: <span id='srv' style='color:#ff9800;'>-</span></div>");
  client.println("<div>State: <span id='run'>-</span></div>");
  client.println("<div>Source: <span id='src' style='color:#1a73e8;'>-</span></div>");
  client.println("</div>");
  client.println("<button id='bon' onclick=\"cmd('/on')\">ON</button>");
  client.println("<button id='boff' onclick=\"cmd('/off')\">OFF</button>");
  client.println("<button class='action' onclick=\"cmd('/center')\">CENTER</button>");
  client.println("<div style='margin-top:16px;font-weight:bold;'>Speed <span id='slv'>0%</span></div>");
  client.println("<input type='range' id='sl' min='0' max='100' value='0'>");
  client.println("</fieldset>");

  // --- LED MATRIX ----------------------------------------------------------
  client.println("<fieldset class='sec'><legend>LED MATRIX</legend>");
  client.println("<button id='bledon' onclick=\"cmd('/led?v=1')\">ON</button>");
  client.println("<button id='bledoff' onclick=\"cmd('/led?v=0')\">OFF</button>");
  client.println("<button class='action' onclick=\"cmd('/ip')\">SHOW IP</button>");
  client.println("<div class='panel'><span id='mtx'>-</span></div>");
  client.println("</fieldset>");

  // --- NETWORK -------------------------------------------------------------
  client.println("<fieldset class='sec'><legend>NETWORK</legend>");
  client.print("<select id='netsel'>");
  for (int i = 0; i < NUM_NETS; i++) {
    client.print("<option value='"); client.print(i); client.print("' data-name='");
    client.print(knownNets[i].ssid); client.print("'>");
    client.print(knownNets[i].ssid); client.print("</option>");
  }
  client.print("<option value='ap' data-name='"); client.print(AP_SSID);
  client.print(" (own network)'>"); client.print(AP_SSID);
  client.println(" (own network)</option></select>");
  client.println("<button class='action' onclick=\"joinNet()\">CONNECT</button>");
  client.println("<div id='netmsg' class='hint'></div>");
  client.println("</fieldset>");

  client.println("<div class='log'><strong>[ LIVE CONSOLE LOG ]</strong><br><br><span id='log'></span></div>");
  client.println("</div>");
  client.println("<script>");
  client.println("var sl=document.getElementById('sl'),drag=false,last=0,netTouched=false;");
  client.println("document.getElementById('netsel').addEventListener('change',function(){netTouched=true;});");
  client.println("function cmd(p){fetch(p).then(refresh);}");
  client.println("function joinNet(){");
  client.println("var s=document.getElementById('netsel');");
  client.println("var v=s.value,name=s.options[s.selectedIndex].text;");
  client.println("fetch(v=='ap'?'/ap':('/join?n='+v));");
  client.println("document.getElementById('netmsg').innerHTML='Switching to <strong>'+name+");
  client.println("'</strong>. This page will stop updating. Reconnect your device to that network, '+");
  client.println("'then browse to the address shown on the LED matrix. If the join fails the board '+");
  client.println("'falls back to its own network.';}");
  client.println("function refresh(){fetch('/status').then(function(r){return r.text();}).then(function(t){");
  client.println("var a=t.split('|~|');");
  client.println("document.getElementById('pct').textContent=a[0]+'%';");
  client.println("document.getElementById('srv').innerHTML=a[1]+'&deg;';");
  client.println("var run=(a[2]=='1');");
  client.println("var r1=document.getElementById('run');");
  client.println("r1.textContent=run?'RUNNING':'STOPPED';");
  client.println("r1.style.color=run?'#34a853':'#ea4335';");
  client.println("document.getElementById('bon').className=run?'on-act':'';");
  client.println("document.getElementById('boff').className=run?'':'off-act';");
  client.println("document.getElementById('src').textContent=a[3];");
  client.println("var ledOn=(a[4]=='1');");
  client.println("document.getElementById('bledon').className=ledOn?'led-act':'';");
  client.println("document.getElementById('bledoff').className=ledOn?'':'off-act';");
  client.println("document.getElementById('mtx').textContent=ledOn?(a[5]||'-'):'(blank)';");
  client.println("document.getElementById('net').textContent=a[6];");
  // Decorate the connected entry and default the control to it, so the dropdown shows
  // where you ARE, not just where you could go. Stop once the user picks something.
  client.println("var cur=a[7],ns=document.getElementById('netsel');");
  client.println("for(var i=0;i<ns.options.length;i++){var o=ns.options[i];");
  client.println("var nm=o.getAttribute('data-name');");
  client.println("o.textContent=(o.value==cur)?('\u25CF '+nm+' - connected'):nm;}");
  client.println("if(!netTouched){ns.value=cur;}");
  client.println("document.getElementById('log').innerHTML=a[8];");
  client.println("if(!drag){sl.value=a[0];document.getElementById('slv').textContent=a[0]+'%';}");
  client.println("});}");
  client.println("sl.addEventListener('pointerdown',function(){drag=true;});");
  client.println("sl.addEventListener('pointerup',function(){drag=false;fetch('/speed?v='+sl.value);});");
  client.println("sl.addEventListener('input',function(){");
  client.println("document.getElementById('slv').textContent=sl.value+'%';");
  client.println("var n=Date.now();if(n-last>150){last=n;fetch('/speed?v='+sl.value);}});");
  client.println("sl.addEventListener('change',function(){fetch('/speed?v='+sl.value);});");
  client.println("setInterval(refresh,1000);refresh();");
  client.println("</script>");
  client.println("</body></html>");
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Wire.begin();
  Modulino.begin();
  knob.begin();
  
  matrix.begin();
  
  myservo.attach(9); // Attach servo to pin 9
  myservo.write(90); // Initialize servo to neutral (center) position
  
  // Initialize knob to 0 for neutral start (0 frequency)
  knob.set(0); 

  logMessage("System Start. Initializing...");
  startJoin(0, false);
}

void loop() {
  int knobPos = knob.get();

  // A physical turn always reclaims control from WiFi.
  if (knobPos != lastKnobPos) {
    lastKnobPos = knobPos;
    controlSource = SRC_KNOB;
    running = true;
  }

  if (knob.isPressed()) {
    knob.set(0);
    knobPos = 0;
    lastKnobPos = 0;
    controlSource = SRC_KNOB;
    running = true;
    // Reset phase on click so it restarts cleanly from center/neutral.
    currentPhase = 0.0; 
    // Double duty: the press is also the only offline way to ask "what is my IP?"
    if (lastIP != "") showBanner(lastIP, 20000);
  }

  int scrollDelay = 135;
  
  // Oscillation Logic
  // The system uses a phase-accumulation based sine wave generator to drive the servo.
  // This approach is chosen over absolute time (sin(f*t)) to prevent sudden jumps
  // in servo position when the frequency is changed by the user.

  // 1. Determine Target Frequency
  // The speed setpoint (0-100) comes from whichever input spoke last: the knob's
  // absolute position, or the value last sent to /speed. It maps linearly to 0-2 Hz.
  activePct = (controlSource == SRC_WIFI) ? (int)wifiPct : abs(knobPos);
  if (activePct > 100) activePct = 100;

  // /off forces the setpoint to zero without discarding it, so /on resumes at the same speed.
  float frequency = running ? ((float)activePct * 2.0 / 100.0) : 0.0;
  
  // Safety Clamp: Ensure frequency never exceeds the physical limit of 2 Hz.
  if (frequency > 2.0) frequency = 2.0;

  servoAngle = 90;
  unsigned long currentTime = millis();
  
  // Initialize lastLoopTime if it's the first run
  if (lastLoopTime == 0) lastLoopTime = currentTime;
  
  // Calculate time delta (dt) in seconds since the last loop iteration.
  float dt = (currentTime - lastLoopTime) / 1000.0;
  lastLoopTime = currentTime;

  if (frequency > 0.01) { 
    // 2. Phase Accumulation
    // Instead of calculating phase = frequency * total_time, we add to the current phase.
    // change_in_phase = frequency * change_in_time * 2PI.
    // This integration ensures the sine wave continues smoothly from its current point
    // even if 'frequency' changes drastically.
    currentPhase += frequency * dt * 2.0 * PI;
    
    // Wrap phase to keep it within standard bounds (0 to 2PI) to prevent float overflow over long periods.
    if (currentPhase > 2.0 * PI) {
      currentPhase -= 2.0 * PI;
    }

    // 3. Calculate Servo Angle
    // sin(currentPhase) yields a value between -1.0 and 1.0.
    // We scale this by MAX_DEVIATION (e.g., 35 degrees) and add it to the neutral 90.
    // Result: An oscillation from (90-35) to (90+35).
    float sineValue = sin(currentPhase);
    servoAngle = 90 + (int)((float)MAX_DEVIATION * sineValue);
    
    // Final safety clamp to protect the servo hardware
    if (servoAngle < 0) servoAngle = 0;
    if (servoAngle > 180) servoAngle = 180;
  } else {
    // If frequency is effectively zero, return to neutral.
    // We also reset the phase so the next movement starts from the standard neutral crossing.
    servoAngle = 90; 
    currentPhase = 0.0; 
  }

  myservo.write(servoAngle);
  
  // 1. MANUAL RENDERER (Window Technique)
  // Once any command banner has expired, fall back to the live status line.
  if (currentTime > bannerUntil) showText(statusString());

  if (currentTime - lastScrollTime >= (unsigned long)scrollDelay) {
    lastScrollTime = currentTime;
    uint32_t frame[3] = {0, 0, 0};
    
    // When the panel is switched off we still push a frame, but leave it all zeros:
    // that is what actually darkens the LEDs. The scroll position freezes, so
    // switching back on resumes mid-word rather than restarting.
    if (matrixOn) {
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 12; c++) {
          if (megaBuffer[r][(windowIndex + c) % megaWidth] > 0) {
            // Arduino LED Matrix expects frames in a specific packed format:
            // 96 bits total (12x8), packed into 3 uint32_t.
            // Mapping (row, col) to linear bit index: standard row-major.
            int bitIndex = (r * 12) + c;
            frame[bitIndex / 32] |= (1 << (31 - (bitIndex % 32))); // MSB first packing
          }
        }
      }
      windowIndex = (windowIndex + 1) % megaWidth;
    }
    matrix.loadFrame(frame);
  }

  // 2. NETWORK SUPERVISION
  if (netMode == NET_JOINING) {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP().toString() == "0.0.0.0") {
      // Associated, but DHCP has not answered yet. Hold the join open rather than
      // announcing 0.0.0.0 and scrolling it across the panel.
      joinDeadline = currentTime + 5000;
    } else if (WiFi.status() == WL_CONNECTED) {
      netMode = NET_STATION;
      lastIP = WiFi.localIP().toString();
      netLabel = String(knownNets[netIndex].ssid) + " " + lastIP;
      logMessage("Joined '" + String(knownNets[netIndex].ssid) + "'. IP: " + lastIP);
      // Hold it long enough to read off the panel and type into a browser.
      showBanner(lastIP, 20000);
    } else if (currentTime > joinDeadline) {
      logMessage("'" + String(knownNets[netIndex].ssid) + "' not in range.");
      if (!joinSingle && netIndex + 1 < NUM_NETS) startJoin(netIndex + 1, false);
      else startAccessPoint(true);
    }
  } else if (netMode == NET_STATION) {
    // A reconnect can come back with a different DHCP lease.
    String currentIP = WiFi.localIP().toString();
    if (currentIP != lastIP && currentIP != "0.0.0.0") {
      lastIP = currentIP;
      netLabel = String(knownNets[netIndex].ssid) + " " + lastIP;
      showBanner(currentIP, 20000);
      logMessage("IP changed: " + currentIP);
    }
  }

  // 3. SERVER START (once, as soon as either mode has an address)
  if (currentTime - lastWiFiCheck > 1000) {
    lastWiFiCheck = currentTime;
    if (!serverStarted && netReady() && lastIP != "" && lastIP != "0.0.0.0") {
      server.begin();
      serverStarted = true;
      logMessage("HTTP server listening on " + lastIP);
    }
  }

  // 4. HTTP: dashboard, /status poll, and the command endpoints
  if (serverStarted) {
    WiFiClient client = server.available();
    if (client) {
      String currentLine = "";
      String reqLine = "";
      bool handled = false;
      unsigned long reqStart = millis();
      // Do NOT gate this on client.connected(). server.available() can hand back a
      // client before its request bytes have landed, and connected() reports false in
      // that window -- so the request gets dropped and the socket closed without a
      // reply, which the caller sees as "connection reset by peer". Wait for data
      // instead, bounded by a timeout so a client that connects and stays silent
      // cannot stall the servo and matrix that share this loop.
      while (!handled && millis() - reqStart < 6000) {
        if (client.available() > 0) {
          char c = client.read();
          if (c == '\n') {
            if (currentLine.length() == 0) {
              // Blank line: headers are done, so route on the request line.
              int sp1 = reqLine.indexOf(' ');
              int sp2 = reqLine.indexOf(' ', sp1 + 1);
              String path = (sp1 >= 0 && sp2 > sp1) ? reqLine.substring(sp1 + 1, sp2) : "/";

              if (path == "/status") {
                sendStatus(client);
              } else if (applyCommand(path)) {
                sendStatus(client); // commands answer with the resulting state
              } else {
                // Only a real page load is logged; the 1 Hz poll would flood it.
                logMessage("Inbound request from: " + client.remoteIP().toString());
                sendDashboard(client);
              }
              handled = true;
            } else {
              // The first non-empty line is "GET /path HTTP/1.1"; the rest are headers.
              if (reqLine.length() == 0) reqLine = currentLine;
              currentLine = "";
            }
          } else if (c != '\r') { currentLine += c; }
        } else {
          // available() is an AT round trip to the modem; pausing keeps us from
          // flooding its command interface while the rest of the request arrives.
          delay(5);
        }
      }
      if (!handled) logMessage("HTTP: request never completed");
      client.stop();
    }
  }

  // 5. DEFERRED NETWORK SWITCHES
  // Handled here rather than in applyCommand() so the HTTP response is fully sent and
  // the socket closed before the interface it travelled over is torn down.
  if (pendingJoin >= 0) { int n = pendingJoin; pendingJoin = -1; startJoin(n, true); }
  if (pendingAP)        { pendingAP = false;   startAccessPoint(false); }
}
