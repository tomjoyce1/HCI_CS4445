// #include <WiFi.h>
// #include <WebSocketsClient.h>
// #include <ArduinoJson.h>
// #include <Wire.h>
// #include <Adafruit_MCP4728.h>
// #include <ESPAsyncWebServer.h>

// // WiFi credentials
// const char* ssid = "ESP32_MPU6050";
// const char* password = "12345678";

// WebSocketsClient webSocket;
// Adafruit_MCP4728 mcp;
// AsyncWebServer server(80);

// // Pin definitions
// const int LED_PIN = D3;
// bool ledState = false;

// // Current values
// float currentYaw = 0.0;
// float currentPitch = 0.0;
// float currentRoll = 0.0;

// // Function to map float values to DAC range (0-4095)
// uint16_t mapFloat(float x, float in_min, float in_max) {
//     return (uint16_t)(((x - in_min) * 4095.0) / (in_max - in_min));
// }

// void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
//     switch(type) {
//         case WStype_DISCONNECTED:
//             Serial.println("Disconnected!");
//             break;
//         case WStype_CONNECTED:
//             Serial.println("Connected!");
//             break;
//         case WStype_TEXT:
//             // Parse incoming JSON data
//             StaticJsonDocument<200> doc;
//             DeserializationError error = deserializeJson(doc, payload);
            
//             if (error) {
//                 Serial.print("deserializeJson() failed: ");
//                 Serial.println(error.c_str());
//                 return;
//             }
            
//             // Extract and store the values
//             currentYaw = doc["yaw"];
//             currentPitch = doc["pitch"];
//             currentRoll = doc["roll"];
            
//             // Map yaw, pitch, roll to DAC values (assuming ranges: yaw±180°, pitch±90°, roll±180°)
//             uint16_t yawValue = mapFloat(currentYaw, -180.0, 180.0);
//             uint16_t pitchValue = mapFloat(currentPitch, -90.0, 90.0);
//             uint16_t rollValue = mapFloat(currentRoll, -180.0, 180.0);
            
//             // Set DAC outputs
//             mcp.setChannelValue(MCP4728_CHANNEL_A, yawValue);    // Yaw on Channel A
//             mcp.setChannelValue(MCP4728_CHANNEL_B, pitchValue);  // Pitch on Channel B
//             mcp.setChannelValue(MCP4728_CHANNEL_C, rollValue);   // Roll on Channel C
            
//             Serial.printf("%.2f, %.2f, %.2f\n", currentYaw, currentPitch, currentRoll);
//             break;
//     }
// }

// const char index_html[] PROGMEM = R"rawliteral(
// <!DOCTYPE HTML>
// <html>
// <head>
//     <title>6DOF Sensor Values</title>
//     <meta name="viewport" content="width=device-width, initial-scale=1">
//     <style>
//         body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }
//         .value-box { 
//             background-color: #f0f0f0;
//             padding: 10px;
//             margin: 10px;
//             border-radius: 5px;
//             display: inline-block;
//             min-width: 150px;
//         }
//         .value { font-size: 24px; font-weight: bold; }
//     </style>
// </head>
// <body>
//     <h2>6DOF Sensor Values</h2>
//     <div class="value-box">
//         <h3>Yaw</h3>
//         <div class="value" id="yaw">0.0\u00B0</div>
//     </div>
//     <div class="value-box">
//         <h3>Pitch</h3>
//         <div class="value" id="pitch">0.0\u00B0</div>
//     </div>
//     <div class="value-box">
//         <h3>Roll</h3>
//         <div class="value" id="roll">0.0\u00B0</div>
//     </div>
//     <script>
//         function updateValues() {
//             fetch('/values')
//                 .then(response => response.json())
//                 .then(data => {
//                     document.getElementById('yaw').textContent = data.yaw.toFixed(2) + '\u00B0';
//                     document.getElementById('pitch').textContent = data.pitch.toFixed(2) + '\u00B0';
//                     document.getElementById('roll').textContent = data.roll.toFixed(2) + '\u00B0';
//                 });
//         }
//         setInterval(updateValues, 100);
//     </script>
// </body>
// </html>
// )rawliteral";

// void setup() {
//     Serial.begin(115200);
//     Wire.begin();

//     // Initialize LED pin
//     pinMode(LED_PIN, OUTPUT);

//     // Initialize MCP4728
//     if (!mcp.begin(0x64)) {
//         Serial.println("Failed to find MCP4728 chip");
//         while (1) {
//             delay(10);
//         }
//     }
//     Serial.println("MCP4728 Found!");

//     // Connect to WiFi
//     WiFi.begin(ssid, password);
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("\nConnected to AP");
//     Serial.print("IP Address: ");
//     Serial.println(WiFi.localIP());

//     // Setup web server routes
//     server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//         request->send_P(200, "text/html", index_html);
//     });

//     server.on("/values", HTTP_GET, [](AsyncWebServerRequest *request){
//         String json = "{\"yaw\":" + String(currentYaw) + 
//                      ",\"pitch\":" + String(currentPitch) + 
//                      ",\"roll\":" + String(currentRoll) + "}";
//         request->send(200, "application/json", json);
//     });

//     // Start server
//     server.begin();

//     // Server IP and port for WebSocket
//     webSocket.begin("192.168.4.1", 81);
//     webSocket.onEvent(webSocketEvent);
    
//     // Initialize all DAC channels to 0
//     mcp.setChannelValue(MCP4728_CHANNEL_A, 0);
//     mcp.setChannelValue(MCP4728_CHANNEL_B, 0);
//     mcp.setChannelValue(MCP4729_CHANNEL_C, 0);
//     mcp.setChannelValue(MCP4728_CHANNEL_D, 0);

// };


// void loop() {
//     webSocket.loop();
// };
