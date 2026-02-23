#include <Arduino.h>
#include <Wire.h>
#include <WiFiS3.h>
#include <Servo.h>
#include "Arduino_Modulino.h"
#include "Arduino_LED_Matrix.h"
#include "config.h"

// --- WIFI CREDENTIALS ---
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;

WiFiServer server(80);
ArduinoLEDMatrix matrix;
ModulinoKnob knob;
Servo myservo; 

// 1. MANUAL FONT DEFINITION (8 rows x 5 columns)
const uint8_t font[12][8] = {
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
  {0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, // .
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}  // space
};

// --- SYSTEM CONSTANTS ---
// Maximum deviation from the neutral 90-degree position.
// The servo will oscillate between (90 - MAX_DEVIATION) and (90 + MAX_DEVIATION).
const int MAX_DEVIATION = 35; 

// 2. SLIDING WINDOW SCROLLER VARIABLES
uint8_t megaBuffer[8][150]; // Large buffer for the full string
int megaWidth = 0;
int windowIndex = 0;
unsigned long lastScrollTime = 0;

void buildMegaBuffer(String msg) {
  megaWidth = 0;
  memset(megaBuffer, 0, sizeof(megaBuffer));
  String fullMsg = "  " + msg + "  "; // Reduced padding from 4 spaces to 2
  for (int i = 0; i < fullMsg.length(); i++) {
    char c = fullMsg[i];
    int fontIdx = 11; 
    if (c >= '0' && c <= '9') fontIdx = c - '0';
    else if (c == '.') fontIdx = 10;
    
    for (int col = 0; col < 5; col++) {
      for (int row = 0; row < 8; row++) {
        if (font[fontIdx][row] & (1 << (4 - col))) megaBuffer[row][megaWidth] = 255;
      }
      megaWidth++;
    }
    megaWidth++; // Character spacing
  }
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
  WiFi.begin(ssid, pass); 
  buildMegaBuffer("SEARCHING");
}

void loop() {
  int knobPos = knob.get();
  
  // Modulino Knob can go negative or beyond 100. Clamp it.
  // Actually, user said "use its absolute value" which implies we might want -50 to mean 50% speed.
  // But they also said "knob position increases frequency".
  // If we clamp 0-100, we ignore negatives. Let's respect "absolute value" instruction more broadly.
  // If we remove the clamp 0-100 for logic, but keep it for display or other things?
  // Let's just use abs() for frequency.
  
  // if (knobPos < 0) knobPos = 0;  <-- REMOVED strict 0 clamp to allow negative interpretation if desired, 
  //                                    but `knob.set(0)` resets it to 0. 
  //                                    If user turns it left from 0, it goes negative.
  //                                    So abs(knobPos) handles the "absolute value" requirement.
  
  // However, we DO need to ensure we don't go wildly out of bounds if that matters.
  // The frequency calculation will use abs(), and we clamp frequency to 2.0.
  // So even if knob is 200, freq is clamped to 2.0.
  
  if (knob.isPressed()) {
    knob.set(0);
    knobPos = 0;
    // Reset phase on click so it restarts cleanly from center/neutral if we want,
    // or we can let it just fade to stop. Resetting phase to 0 means sin(0)=0 -> offset 0 -> 90 degrees.
    currentPhase = 0.0; 
  }

  // Optional: If you want to force the physical knob value back to the range (feedback),
  // you can try knob.set(knobPos); but simply clamping the local variable is safer/faster for now.

  int scrollDelay = 135;
  
  // Oscillation Logic
  // The system uses a phase-accumulation based sine wave generator to drive the servo.
  // This approach is chosen over absolute time (sin(f*t)) to prevent sudden jumps
  // in servo position when the frequency is changed by the user.

  // 1. Determine Target Frequency
  // The knob position (0 to 100 or 0 to -100) maps linearly to a frequency of 0 to 2 Hz.
  // We use abs(knobPos) so turning the knob in either direction increases the speed.
  float frequency = (float)abs(knobPos) * 2.0 / 100.0;
  
  // Safety Clamp: Ensure frequency never exceeds the physical limit of 2 Hz.
  if (frequency > 2.0) frequency = 2.0;

  int servoAngle = 90;
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
  if (currentTime - lastScrollTime >= (unsigned long)scrollDelay) {
    lastScrollTime = currentTime;
    uint32_t frame[3] = {0, 0, 0};
    
    if (megaWidth > 0) {
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 12; c++) {
          if (megaBuffer[r][(windowIndex + c) % megaWidth] > 0) {
            // Arduino LED Matrix expects frames in a specific packed format:
            // 96 bits total (12x8), packed into 3 uint32_t.
            // Mapping (row, col) to linear bit index:
            // The library documentation / examples typically map this linearly.
            // Let's try standard row-major mapping: bitIndex = r * 12 + c
            // Note: Some matrices map differently, but let's start with standard.
            int bitIndex = (r * 12) + c;
            frame[bitIndex / 32] |= (1 << (31 - (bitIndex % 32))); // MSB first packing
          }
        }
      }
    }
    matrix.loadFrame(frame);
    windowIndex = (windowIndex + 1) % megaWidth;
  }

  // 2. IP MONITOR
  String currentIP = WiFi.localIP().toString();
  if (WiFi.status() == WL_CONNECTED && currentIP != lastIP && currentIP != "0.0.0.0") {
    lastIP = currentIP;
    buildMegaBuffer(currentIP);
    logMessage("WiFi Connected! IP: " + currentIP);
  }

  // 3. WIFI BACKGROUND CHECK
  if (currentTime - lastWiFiCheck > 5000) {
    lastWiFiCheck = currentTime;
    if (WiFi.status() == WL_CONNECTED && !serverStarted && lastIP != "") {
        server.begin();
        serverStarted = true;
    }
  }

  // 4. PREVIOUS "WIDE" DASHBOARD HTML (No changes)
  if (serverStarted) {
    WiFiClient client = server.available();
    if (client) {
      logMessage("Inbound request from: " + client.remoteIP().toString());
      String currentLine = "";
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          if (c == '\n') {
            if (currentLine.length() == 0) {
              client.println("HTTP/1.1 200 OK\nContent-type:text/html\nConnection: close\n");
              client.println("<!DOCTYPE HTML><html><head>");
              client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
              client.println("<style>");
              client.println("body { font-family: sans-serif; text-align: center; background-color: #f0f2f5; padding: 10px; color: #333; }");
              client.println(".card { background: white; padding: 20px; border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.1); display: block; margin: auto; max-width: 800px; }");
              client.println(".log { text-align: left; background: #000; padding: 20px; border-radius: 8px; height: 400px; overflow-y: auto; font-family: 'Courier New', monospace; color: #33ff33; margin-top: 20px; font-size: 14px; border: 2px solid #444; }");
              client.println("h1 { color: #1a73e8; }");
              client.println(".data-row { display: flex; justify-content: space-around; font-size: 1.2em; margin: 15px 0; font-weight: bold; }");
              client.println("</style></head><body>");
              client.println("<div class='card'>");
              client.println("<h1>R4 System Control</h1>");
              client.println("<div class='data-row'>");
              client.println("<div>Knob: <span style='color:#e91e63;'>" + String(knobPos) + "%</span></div>");
              client.println("<div>Servo: <span style='color:#ff9800;'>" + String(servoAngle) + "&deg;</span></div>");
              client.println("<div>Scroll: <span style='color:#1a73e8;'>" + String(scrollDelay) + "ms</span></div>");
              client.println("</div>");
              client.println("<div class='log'><strong>[ LIVE CONSOLE LOG ]</strong><br><br>" + webLog + "</div>");
              client.println("</div>");
              client.println("<script>setTimeout(function(){location.reload();}, 2000);</script>");
              client.println("</body></html>");
              break;
            } else { currentLine = ""; }
          } else if (c != '\r') { currentLine += c; }
        }
      }
      client.stop();
    }
  }
}
