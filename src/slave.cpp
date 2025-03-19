#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_MCP4728.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

// WiFi credentials
const char* ssid = "ESP32_MPU6050";
const char* password = "12345678";

WebSocketsClient webSocket;
Adafruit_MCP4728 mcp;
AsyncWebServer server(80);

// Pin definitions
const int LED_PIN = D3;
bool ledState = false;

// Channel mapping - default configuration
MCP4728_channel_t yawChannel = MCP4728_CHANNEL_A;    // Default: Yaw on Channel A
MCP4728_channel_t pitchChannel = MCP4728_CHANNEL_B;  // Default: Pitch on Channel B
MCP4728_channel_t rollChannel = MCP4728_CHANNEL_C;   // Default: Roll on Channel C
// Channel D is unused by default

// Ring buffer for data smoothing
const int BUFFER_SIZE = 2; // Small buffer for minimal latency
float yawBuffer[BUFFER_SIZE] = {0};
float pitchBuffer[BUFFER_SIZE] = {0};
float rollBuffer[BUFFER_SIZE] = {0};
int bufferIndex = 0;

// Current values
float currentYaw = 0.0;
float currentPitch = 0.0;
float currentRoll = 0.0;

// Last update time for rate limiting
unsigned long lastUpdateTime = 0;
const unsigned long UPDATE_INTERVAL = 20; // 50Hz update rate

// Function to map float values to DAC range (0-4095)
uint16_t mapFloat(float x, float in_min, float in_max) {
    return (uint16_t)(((x - in_min) * 4095.0) / (in_max - in_min));
}

// Quick smoothing function
float smoothValue(float* buffer, float newValue) {
    // Add new value to buffer
    buffer[bufferIndex] = newValue;
    
    // Calculate average
    float sum = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        sum += buffer[i];
    }
    
    return sum / BUFFER_SIZE;
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("Disconnected!");
            break;
        case WStype_CONNECTED:
            Serial.println("Connected!");
            // Clear buffer on new connection
            for (int i = 0; i < BUFFER_SIZE; i++) {
                yawBuffer[i] = 0;
                pitchBuffer[i] = 0;
                rollBuffer[i] = 0;
            }
            break;
        case WStype_TEXT:
            // Rate limit processing to prevent flooding
            unsigned long currentTime = millis();
            if (currentTime - lastUpdateTime < UPDATE_INTERVAL) {
                return; // Skip this update to maintain consistent rate
            }
            lastUpdateTime = currentTime;
            
            // Parse incoming JSON data - use smaller document size
            StaticJsonDocument<96> doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (error) {
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.c_str());
                return;
            }
            
            // Extract and store the values
            float rawYaw = doc["yaw"];
            float rawPitch = doc["pitch"];
            float rawRoll = doc["roll"];
            
            // Update buffer index for next smoothing operation
            bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
            
            // Apply minimal smoothing
            currentYaw = smoothValue(yawBuffer, rawYaw);
            currentPitch = smoothValue(pitchBuffer, rawPitch);
            currentRoll = smoothValue(rollBuffer, rawRoll);
            
            // Map yaw, pitch, roll to DAC values (assuming ranges: yaw±180°, pitch±90°, roll±180°)
            uint16_t yawValue = mapFloat(currentYaw, -180.0, 180.0);
            uint16_t pitchValue = mapFloat(currentPitch, -90.0, 90.0);
            uint16_t rollValue = mapFloat(currentRoll, -180.0, 180.0);
            
            // Set DAC outputs according to user-configured channel mapping
            mcp.setChannelValue(yawChannel, yawValue);      // Yaw
            mcp.setChannelValue(pitchChannel, pitchValue);  // Pitch
            mcp.setChannelValue(rollChannel, rollValue);    // Roll
            
            Serial.printf("%.2f, %.2f, %.2f\n", currentYaw, currentPitch, currentRoll);
            break;
    }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>6DOF Sensor Values</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        :root {
            --primary-color: #3498db;
            --secondary-color: #2c3e50;
            --accent-color: #e74c3c;
            --background-color: #f9f9f9;
            --card-background: #ffffff;
            --text-color: #333333;
        }
        
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: var(--background-color);
            color: var(--text-color);
            line-height: 1.6;
            padding: 20px;
            max-width: 100%;
            overflow-x: hidden;
        }
        
        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 15px;
        }
        
        header {
            text-align: center;
            margin-bottom: 30px;
        }
        
        h1 {
            color: var(--secondary-color);
            font-size: 28px;
            margin-bottom: 10px;
        }
        
        .subtitle {
            color: var(--primary-color);
            font-size: 16px;
            margin-bottom: 20px;
        }
        
        .values-container {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 15px;
            margin-bottom: 30px;
        }
        
        .value-card {
            background-color: var(--card-background);
            border-radius: 12px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            padding: 20px;
            flex: 1 1 250px;
            max-width: 100%;
            transition: transform 0.2s ease;
            position: relative;
            overflow: hidden;
        }
        
        .value-card:hover {
            transform: translateY(-5px);
        }
        
        .value-card h2 {
            color: var(--primary-color);
            font-size: 20px;
            margin-bottom: 15px;
            text-align: center;
        }
        
        .value-display {
            text-align: center;
            font-size: 32px;
            font-weight: bold;
            color: var(--secondary-color);
            margin-bottom: 15px;
        }
        
        .value-indicator {
            height: 10px;
            background-color: #eee;
            border-radius: 5px;
            overflow: hidden;
            margin-top: 10px;
        }
        
        .indicator-fill {
            height: 100%;
            background-color: var(--primary-color);
            width: 50%;
            transition: width 0.3s ease;
        }
        
        .yaw-indicator .indicator-fill {
            background-color: #3498db;
        }
        
        .pitch-indicator .indicator-fill {
            background-color: #2ecc71;
        }
        
        .roll-indicator .indicator-fill {
            background-color: #e74c3c;
        }

        .settings-card {
            background-color: var(--card-background);
            border-radius: 12px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            padding: 20px;
            margin-bottom: 30px;
        }
        
        .settings-card h2 {
            color: var(--secondary-color);
            font-size: 20px;
            margin-bottom: 15px;
            text-align: center;
        }
        
        .channel-config {
            display: grid;
            grid-template-columns: 1fr 2fr;
            gap: 10px;
            margin-bottom: 15px;
        }
        
        .channel-config label {
            font-weight: bold;
            display: flex;
            align-items: center;
        }
        
        select {
            padding: 8px;
            border-radius: 4px;
            border: 1px solid #ddd;
            background-color: white;
            width: 100%;
        }
        
        button {
            background-color: var(--primary-color);
            color: white;
            border: none;
            padding: 10px 15px;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            transition: background-color 0.2s;
            width: 100%;
            margin-top: 10px;
        }
        
        button:hover {
            background-color: #2980b9;
        }
        
        .test-btn {
            background-color: #95a5a6;
            font-size: 14px;
            padding: 8px;
        }
        
        .test-btn:hover {
            background-color: #7f8c8d;
        }
        
        .orientation-visualizer {
            width: 100%;
            aspect-ratio: 1;
            max-width: 300px;
            margin: 0 auto 30px auto;
            position: relative;
            border-radius: 50%;
            background-color: var(--card-background);
            box-shadow: 0 4px 8px rgba(0,0,0,0.1);
            overflow: hidden;
        }
        
        .orientation-circle {
            position: absolute;
            top: 50%;
            left: 50%;
            width: 80%;
            height: 80%;
            transform: translate(-50%, -50%);
            border-radius: 50%;
            border: 2px solid var(--primary-color);
            display: flex;
            align-items: center;
            justify-content: center;
        }
        
        .orientation-circle::after {
            content: '';
            position: absolute;
            width: 10px;
            height: 10px;
            background-color: var(--accent-color);
            border-radius: 50%;
        }
        
        .crosshair {
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
        }
        
        .crosshair::before, .crosshair::after {
            content: '';
            position: absolute;
            background-color: rgba(0,0,0,0.1);
        }
        
        .crosshair::before {
            width: 1px;
            height: 100%;
            left: 50%;
        }
        
        .crosshair::after {
            height: 1px;
            width: 100%;
            top: 50%;
        }
        
        .dot {
            position: absolute;
            top: 50%;
            left: 50%;
            width: 20px;
            height: 20px;
            margin-left: -10px;
            margin-top: -10px;
            background-color: var(--accent-color);
            border-radius: 50%;
            transform: translate(0, 0);
            transition: transform 0.2s ease;
        }
        
        @media (max-width: 600px) {
            .values-container {
                flex-direction: column;
            }
            
            .value-card {
                max-width: 100%;
            }
            
            h1 {
                font-size: 24px;
            }
            
            .value-display {
                font-size: 28px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>6DOF Sensor Dashboard</h1>
            <p class="subtitle">Real-time orientation data</p>
        </header>
        
        <div class="orientation-visualizer">
            <div class="crosshair"></div>
            <div class="orientation-circle"></div>
            <div class="dot" id="orientation-dot"></div>
        </div>
        
        <div class="settings-card">
            <h2>Channel Configuration</h2>
            <div class="channel-config">
                <label for="yaw-channel">Yaw Output:</label>
                <select id="yaw-channel">
                    <option value="0">Channel A</option>
                    <option value="1">Channel B</option>
                    <option value="2">Channel C</option>
                    <option value="3">Channel D</option>
                </select>
            </div>
            <div class="channel-config">
                <label for="pitch-channel">Pitch Output:</label>
                <select id="pitch-channel">
                    <option value="0">Channel A</option>
                    <option value="1" selected>Channel B</option>
                    <option value="2">Channel C</option>
                    <option value="3">Channel D</option>
                </select>
            </div>
            <div class="channel-config">
                <label for="roll-channel">Roll Output:</label>
                <select id="roll-channel">
                    <option value="0">Channel A</option>
                    <option value="1">Channel B</option>
                    <option value="2" selected>Channel C</option>
                    <option value="3">Channel D</option>
                </select>
            </div>
            <button id="save-config">Save Configuration</button>
            
            <h3 style="margin-top: 20px; text-align: center;">Test Channels</h3>
            <p style="text-align: center; font-size: 14px; margin-bottom: 15px;">
                Click to send a test pulse to each channel for identification
            </p>
            <div style="display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px;">
                <button onclick="testChannel(0)" class="test-btn">Test A</button>
                <button onclick="testChannel(1)" class="test-btn">Test B</button>
                <button onclick="testChannel(2)" class="test-btn">Test C</button>
                <button onclick="testChannel(3)" class="test-btn">Test D</button>
            </div>
        </div>
        
        <div class="values-container">
            <div class="value-card">
                <h2>Yaw</h2>
                <div class="value-display" id="yaw">0.0\u00B0</div>
                <div class="value-indicator yaw-indicator">
                    <div class="indicator-fill" id="yaw-fill"></div>
                </div>
            </div>
            
            <div class="value-card">
                <h2>Pitch</h2>
                <div class="value-display" id="pitch">0.0\u00B0</div>
                <div class="value-indicator pitch-indicator">
                    <div class="indicator-fill" id="pitch-fill"></div>
                </div>
            </div>
            
            <div class="value-card">
                <h2>Roll</h2>
                <div class="value-display" id="roll">0.0\u00B0</div>
                <div class="value-indicator roll-indicator">
                    <div class="indicator-fill" id="roll-fill"></div>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        // Elements
        const yawElement = document.getElementById('yaw');
        const pitchElement = document.getElementById('pitch');
        const rollElement = document.getElementById('roll');
        const yawFill = document.getElementById('yaw-fill');
        const pitchFill = document.getElementById('pitch-fill');
        const rollFill = document.getElementById('roll-fill');
        const orientationDot = document.getElementById('orientation-dot');
        
        // Channel configuration elements
        const yawChannelSelect = document.getElementById('yaw-channel');
        const pitchChannelSelect = document.getElementById('pitch-channel');
        const rollChannelSelect = document.getElementById('roll-channel');
        const saveConfigButton = document.getElementById('save-config');
        
        // Add event listener for saving configuration
        saveConfigButton.addEventListener('click', function() {
            const yawChannel = parseInt(yawChannelSelect.value);
            const pitchChannel = parseInt(pitchChannelSelect.value);
            const rollChannel = parseInt(rollChannelSelect.value);
            
            // Check for duplicate channel assignments
            const channels = [yawChannel, pitchChannel, rollChannel];
            const uniqueChannels = [...new Set(channels)];
            
            if (uniqueChannels.length < channels.length) {
                alert('Error: Each axis must use a different channel!');
                return;
            }
            
            // Send configuration to server
            fetch('/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    yaw: yawChannel,
                    pitch: pitchChannel,
                    roll: rollChannel
                })
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error('Network response was not ok');
                }
                return response.json();
            })
            .then(data => {
                if (data.success) {
                    alert('Configuration saved successfully!');
                } else {
                    alert('Error saving configuration');
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Error saving configuration');
            });
        });
        
        // Function to get current configuration
        function getCurrentConfig() {
            fetch('/config')
                .then(response => {
                    if (!response.ok) {
                        throw new Error('Network response was not ok');
                    }
                    return response.json();
                })
                .then(data => {
                    yawChannelSelect.value = data.yaw;
                    pitchChannelSelect.value = data.pitch;
                    rollChannelSelect.value = data.roll;
                })
                .catch(error => {
                    console.error('Error fetching config:', error);
                });
        }
        
        // Function to test a specific channel
        function testChannel(channel) {
            fetch('/test-channel?channel=' + channel, { method: 'POST' })
                .then(response => {
                    if (!response.ok) {
                        throw new Error('Failed to test channel');
                    }
                    return response.json();
                })
                .then(data => {
                    console.log('Testing channel ' + channel);
                })
                .catch(error => {
                    console.error('Error testing channel:', error);
                });
        }
        
        // Value ranges
        const YAW_RANGE = 360; // -180 to 180
        const PITCH_RANGE = 180; // -90 to 90
        const ROLL_RANGE = 360; // -180 to 180
        
        // Initialize channel configuration
        getCurrentConfig();
        
        function updateValues() {
            fetch('/values')
                .then(response => {
                    if (!response.ok) {
                        throw new Error('Network response was not ok');
                    }
                    return response.json();
                })
                .then(data => {
                    // Update text displays
                    yawElement.textContent = data.yaw.toFixed(1) + '\u00B0';
                    pitchElement.textContent = data.pitch.toFixed(1) + '\u00B0';
                    rollElement.textContent = data.roll.toFixed(1) + '\u00B0';
                    
                    // Update indicators
                    const yawPercent = ((data.yaw + 180) / YAW_RANGE * 100);
                    const pitchPercent = ((data.pitch + 90) / PITCH_RANGE * 100);
                    const rollPercent = ((data.roll + 180) / ROLL_RANGE * 100);
                    
                    yawFill.style.width = yawPercent + '%';
                    pitchFill.style.width = pitchPercent + '%';
                    rollFill.style.width = rollPercent + '%';
                    
                    // Update orientation visualizer (simplified)
                    // Using pitch and roll for visualization (ignoring yaw for simplicity)
                    const pitchOffset = Math.max(Math.min(data.pitch, 45), -45) / 45 * 30;
                    const rollOffset = Math.max(Math.min(data.roll, 45), -45) / 45 * 30;
                    
                    orientationDot.style.transform = `translate(${rollOffset}px, ${pitchOffset}px)`;
                })
                .catch(error => {
                    console.error('Error fetching data:', error);
                });
        }
        
        // Initial update
        updateValues();
        
        // Set update interval
        setInterval(updateValues, 100);
    </script>
</body>
</html>
)rawliteral";

// Utility function to convert numeric value to MCP4728_channel_t enum
MCP4728_channel_t numToChannel(int channelNum) {
    switch(channelNum) {
        case 0: return MCP4728_CHANNEL_A;
        case 1: return MCP4728_CHANNEL_B;
        case 2: return MCP4728_CHANNEL_C;
        case 3: return MCP4728_CHANNEL_D;
        default: return MCP4728_CHANNEL_A; // Default fallback
    }
}

// Function to get channel number from enum
int channelToNum(MCP4728_channel_t channel) {
    switch(channel) {
        case MCP4728_CHANNEL_A: return 0;
        case MCP4728_CHANNEL_B: return 1;
        case MCP4728_CHANNEL_C: return 2;
        case MCP4728_CHANNEL_D: return 3;
        default: return 0; // Default fallback
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();

    // Initialize LED pin
    pinMode(LED_PIN, OUTPUT);

    // Initialize MCP4728
    if (!mcp.begin(0x64)) {
        Serial.println("Failed to find MCP4728 chip");
        while (1) {
            delay(10);
        }
    }
    Serial.println("MCP4728 Found!");

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to AP");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Setup web server routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.on("/values", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"yaw\":" + String(currentYaw) + 
                     ",\"pitch\":" + String(currentPitch) + 
                     ",\"roll\":" + String(currentRoll) + "}";
        request->send(200, "application/json", json);
    });

    // Add endpoint for getting current channel configuration
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"yaw\":" + String(channelToNum(yawChannel)) + 
                     ",\"pitch\":" + String(channelToNum(pitchChannel)) + 
                     ",\"roll\":" + String(channelToNum(rollChannel)) + "}";
        request->send(200, "application/json", json);
    });

    // Add endpoint for testing a specific channel
    server.on("/test-channel", HTTP_POST, [](AsyncWebServerRequest *request){
        int channel = 0;
        if (request->hasParam("channel")) {
            channel = request->getParam("channel")->value().toInt();
            channel = constrain(channel, 0, 3);
            
            // Send pulse to the requested channel
            MCP4728_channel_t testChannel = numToChannel(channel);
            
            // First store current values by recording what's being sent to the DAC
            uint16_t yawVal = mapFloat(currentYaw, -180.0, 180.0);
            uint16_t pitchVal = mapFloat(currentPitch, -90.0, 90.0);
            uint16_t rollVal = mapFloat(currentRoll, -180.0, 180.0);
            
            // Send a pulse to the requested channel
            mcp.setChannelValue(testChannel, 4095); // Full voltage
            delay(500); // Pulse for 500ms
            
            // Restore normal operation by setting channel values based on their mapping
            mcp.setChannelValue(yawChannel, yawVal);
            mcp.setChannelValue(pitchChannel, pitchVal);
            mcp.setChannelValue(rollChannel, rollVal);
            
            Serial.printf("Tested channel %d\n", channel);
            
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Channel parameter required\"}");
        }
    });

    // Add endpoint for updating channel configuration
    AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/config", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StaticJsonDocument<200> data;
        if (json.is<JsonObject>()) {
            data = json.as<JsonObject>();
            
            // Get values from JSON with appropriate bounds checking
            int yawVal = data["yaw"] | channelToNum(yawChannel);  // Default to current if missing
            int pitchVal = data["pitch"] | channelToNum(pitchChannel);
            int rollVal = data["roll"] | channelToNum(rollChannel);
            
            // Ensure values are in valid range (0-3)
            yawVal = constrain(yawVal, 0, 3);
            pitchVal = constrain(pitchVal, 0, 3);
            rollVal = constrain(rollVal, 0, 3);
            
            // Update channel mapping
            yawChannel = numToChannel(yawVal);
            pitchChannel = numToChannel(pitchVal);
            rollChannel = numToChannel(rollVal);
            
            // For debug
            Serial.printf("Updated channel mapping: Yaw=%d, Pitch=%d, Roll=%d\n", 
                        yawVal, pitchVal, rollVal);
            
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        }
    });
    server.addHandler(handler);

    // Start server
    server.begin();

    // Server IP and port for WebSocket
    webSocket.begin("192.168.4.1", 81);
    webSocket.onEvent(webSocketEvent);
    
    // Initialize all DAC channels to 0
    mcp.setChannelValue(MCP4728_CHANNEL_A, 0);
    mcp.setChannelValue(MCP4728_CHANNEL_B, 0);
    mcp.setChannelValue(MCP4728_CHANNEL_C, 0);
    mcp.setChannelValue(MCP4728_CHANNEL_D, 0);
}

void loop() {
    webSocket.loop();
    
    // Add a small delay to prevent CPU hogging while still being responsive
    delay(1);
}
