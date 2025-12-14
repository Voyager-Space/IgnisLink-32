#include <WiFi.h>
#include <WebServer.h>

// ================= CONFIGURATION =================
const char* ssid = "RocketCommand";    // Network Name
const char* password = "GoForLaunch";   // Network Password
const int COUNTDOWN_SECONDS = 5;
const int IGNITION_DURATION_MS = 3000; // 3 Seconds MAX

// ================= PIN DEFINITIONS =================

const int PIN_RELAY = 26;
const int PIN_BUZZER = 25;  
const int PIN_LED_RED = 14;  
const int PIN_LED_GREEN = 27;

// ================= STATE MACHINE =================
enum SystemState {
  STATE_IDLE,
  STATE_COUNTDOWN,
  STATE_FIRING
};

SystemState currentState = STATE_IDLE;
unsigned long stateStartTime = 0;
unsigned long lastTickTime = 0;
int countdownCounter = 0;

WebServer server(80);

// ================= WEB INTERFACE (HTML/CSS/JS) =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <title>ROCKET COMMAND</title>
  <style>
    body { background-color: #121212; color: #00ff00; font-family: 'Courier New', monospace; text-align: center; margin: 0; padding: 20px; touch-action: manipulation; }
    h1 { letter-spacing: 2px; border-bottom: 2px solid #00ff00; padding-bottom: 10px; }
    #status-box { border: 2px solid #333; padding: 20px; margin: 20px auto; width: 80%; border-radius: 10px; background: #1a1a1a; }
    #timer { font-size: 80px; font-weight: bold; color: #fff; margin: 20px 0; }
    .btn { background: #cc0000; color: white; border: none; padding: 25px 50px; font-size: 24px; font-weight: bold; border-radius: 50%; width: 200px; height: 200px; cursor: pointer; box-shadow: 0 0 20px rgba(255,0,0,0.5); outline: none; -webkit-tap-highlight-color: transparent; }
    .btn:active { background: #990000; transform: scale(0.95); box-shadow: 0 0 10px rgba(255,0,0,0.8); }
    .btn:disabled { background: #333; color: #666; box-shadow: none; cursor: not-allowed; }
    .info { font-size: 14px; color: #666; margin-top: 30px; }
  </style>
</head>
<body>
  <h1>LAUNCH CONTROL</h1>
  
  <div id="status-box">
    <div>SYSTEM STATUS</div>
    <div id="status-text" style="font-size: 24px; margin-top: 10px;">READY</div>
  </div>

  <div id="timer">00</div>

  <button id="launchBtn" class="btn" onclick="initiateLaunch()">LAUNCH</button>

  <div class="info">SYSTEM ID: ESP32-IGNITION-V1</div>

  <script>
    function initiateLaunch() {
      document.getElementById("launchBtn").disabled = true;
      document.getElementById("status-text").innerHTML = "COUNTDOWN INITIATED";
      document.getElementById("status-text").style.color = "yellow";
      fetch('/trigger').then(resp => {
        if(resp.ok) startLocalTimer();
      }).catch(err => {
        alert("COMMUNICATION ERROR: " + err);
        resetUI();
      });
    }

    function startLocalTimer() {
      let count = 5;
      document.getElementById("timer").innerHTML = "0" + count;
      
      let interval = setInterval(() => {
        count--;
        if(count >= 0) {
          document.getElementById("timer").innerHTML = "0" + count;
        } 
        
        if(count === 0) {
           document.getElementById("status-text").innerHTML = "IGNITION ACTIVE";
           document.getElementById("status-text").style.color = "red";
           document.getElementById("timer").style.color = "red";
        }

        if(count < -3) { // 3 seconds after 0
          clearInterval(interval);
          resetUI();
        }
      }, 1000);
    }

    function resetUI() {
      document.getElementById("launchBtn").disabled = false;
      document.getElementById("status-text").innerHTML = "READY";
      document.getElementById("status-text").style.color = "#00ff00";
      document.getElementById("timer").innerHTML = "00";
      document.getElementById("timer").style.color = "white";
    }
  </script>
</body>
</html>
)rawliteral";

// ================= SETUP =================
void setup() {
  Serial.begin(115200);


  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW); 

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_LED_RED, OUTPUT);
  digitalWrite(PIN_LED_RED, LOW);

  pinMode(PIN_LED_GREEN, OUTPUT);
  digitalWrite(PIN_LED_GREEN, HIGH); 


  Serial.println("Creating Access Point...");
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());


  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/trigger", HTTP_GET, []() {
    if (currentState == STATE_IDLE) {
      Serial.println("[CMD] Launch Sequence Initiated");
      currentState = STATE_COUNTDOWN;
      countdownCounter = COUNTDOWN_SECONDS;
      lastTickTime = millis();
      server.send(200, "text/plain", "ACK");
    } else {
      server.send(409, "text/plain", "BUSY");
    }
  });

  server.begin();
}

// ================= MAIN LOOP =================
void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  switch (currentState) {
    
    case STATE_IDLE:
      
      digitalWrite(PIN_LED_GREEN, HIGH);
      digitalWrite(PIN_RELAY, LOW); 
      break;

    case STATE_COUNTDOWN:
      digitalWrite(PIN_LED_GREEN, LOW); 

    
      if (currentMillis - lastTickTime >= 1000) {
        lastTickTime = currentMillis;
        
        if (countdownCounter > 0) {
          
          Serial.printf("T-Minus %d\n", countdownCounter);
          tone(PIN_BUZZER, 2000, 200); 
          
          
          digitalWrite(PIN_LED_RED, HIGH);
          delay(100); 
          digitalWrite(PIN_LED_RED, LOW);
          
          countdownCounter--;
        } else {
          
          currentState = STATE_FIRING;
          stateStartTime = currentMillis;
        }
      }
      break;

    case STATE_FIRING:
      digitalWrite(PIN_RELAY, HIGH);
      digitalWrite(PIN_LED_RED, HIGH); 
      digitalWrite(PIN_BUZZER, HIGH);  

      
      if (currentMillis - stateStartTime >= IGNITION_DURATION_MS) {
        Serial.println("[SAFETY] Auto-Cutoff Triggered");
        
       
        digitalWrite(PIN_RELAY, LOW);
        digitalWrite(PIN_BUZZER, LOW);
        digitalWrite(PIN_LED_RED, LOW);
        digitalWrite(PIN_LED_GREEN, HIGH);
        
        currentState = STATE_IDLE;
      }
      break;
  }
}
