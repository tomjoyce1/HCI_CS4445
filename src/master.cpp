// #include "I2Cdev.h"
// #include "MPU6050_6Axis_MotionApps20.h"
// #include <WiFi.h>
// #include <WebSocketsServer.h>
// #include <ArduinoJson.h>
// #include <Adafruit_MCP4728.h>

// // WiFi credentials
// const char* ssid = "ESP32_MPU6050";
// const char* password = "12345678";

// WebSocketsServer webSocket = WebSocketsServer(81);

// // MPU6050 instance
// MPU6050 mpu;
// Adafruit_MCP4728 mcp;

// // Pin definitions
// const int INTERRUPT_PIN = 2;
// const int BUTTON_PIN = D2;
// const int LED_PIN = D3;
// const long SLOW_BLINK = 1000;
// const long FAST_BLINK = 100;
// unsigned long previousMillis = 0;
// bool ledState = false;
// bool initializationStarted = false;

// // MPU control/status vars
// bool DMPReady = false;
// uint8_t MPUIntStatus;
// uint8_t devStatus;
// uint16_t packetSize;
// uint8_t FIFOBuffer[64];

// // Orientation/motion vars
// Quaternion q;
// VectorInt16 aa;
// VectorInt16 gy;
// VectorInt16 aaReal;
// VectorInt16 aaWorld;
// VectorFloat gravity;
// float euler[3];
// float ypr[3];

// // Interrupt detection routine
// volatile bool MPUInterrupt = false;
// void DMPDataReady() {
//     MPUInterrupt = true;
// }

// // WebSocket event handler
// void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
//     switch(type) {
//         case WStype_DISCONNECTED:
//             Serial.printf("[%u] Disconnected!\n", num);
//             break;
//         case WStype_CONNECTED:
//             Serial.printf("[%u] Connected!\n", num);
//             break;
//     }
// }

// void setup() {
//     Wire.begin();
//     Wire.setClock(400000);
    
//     Serial.begin(115200);
    
//     // Initialize button and LED pins
//     pinMode(BUTTON_PIN, INPUT_PULLUP);
//     pinMode(LED_PIN, OUTPUT);

//     // Initialize WiFi AP
//     WiFi.softAP(ssid, password);
//     Serial.println("Access Point Started");
//     Serial.print("IP Address: ");
//     Serial.println(WiFi.softAPIP());
    
//     // Start WebSocket server
//     webSocket.begin();
//     webSocket.onEvent(webSocketEvent);

//     // Initialize MPU6050
//     Serial.println(F("Initializing I2C devices..."));
//     mpu.initialize();
//     pinMode(INTERRUPT_PIN, INPUT);

//     if(mpu.testConnection() == false){
//         Serial.println("MPU6050 connection failed");
//         while(true);
//     }
//     Serial.println("MPU6050 connection successful");

//     // Wait for button press to initialize DMP
//     Serial.println(F("Press the button to begin initialization..."));
//     while(digitalRead(BUTTON_PIN) == HIGH) {
//         unsigned long currentMillis = millis();
//         if (currentMillis - previousMillis >= SLOW_BLINK) {
//             previousMillis = currentMillis;
//             ledState = !ledState;
//             digitalWrite(LED_PIN, ledState);
//         }
//         delay(10);
//     }
//     delay(50);
    
//     initializationStarted = true;

//     Serial.println(F("Initializing DMP..."));
//     devStatus = mpu.dmpInitialize();

//     mpu.setXGyroOffset(0);
//     mpu.setYGyroOffset(0);
//     mpu.setZGyroOffset(0);
//     mpu.setXAccelOffset(0);
//     mpu.setYAccelOffset(0);
//     mpu.setZAccelOffset(0);

//     if (devStatus == 0) {
//         unsigned long calibrationStart = millis();
//         Serial.println("Starting calibration...");
        
//         while(millis() - calibrationStart < 6000) {
//             unsigned long currentMillis = millis();
//             if (currentMillis - previousMillis >= FAST_BLINK) {
//                 previousMillis = currentMillis;
//                 ledState = !ledState;
//                 digitalWrite(LED_PIN, ledState);
//             }
            
//             if ((millis() - calibrationStart) < 3000) {
//                 mpu.CalibrateAccel(1);
//             } else {
//                 mpu.CalibrateGyro(1);
//             }
//         }
        
//         Serial.println("These are the Active offsets: ");
//         mpu.PrintActiveOffsets();
        
//         Serial.println(F("Enabling DMP..."));
//         mpu.setDMPEnabled(true);

//         attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), DMPDataReady, RISING);
//         MPUIntStatus = mpu.getIntStatus();
        
//         DMPReady = true;
//         digitalWrite(LED_PIN, HIGH);
//     } else {
//         Serial.print(F("DMP Initialization failed (code "));
//         Serial.print(devStatus);
//         Serial.println(F(")"));
//     }
// }

// void loop() {
//     if (!initializationStarted) {
//         return;
//     }
    
//     if (!DMPReady) {
//         unsigned long currentMillis = millis();
//         if (currentMillis - previousMillis >= FAST_BLINK) {
//             previousMillis = currentMillis;
//             ledState = !ledState;
//             digitalWrite(LED_PIN, ledState);
//         }
//         return;
//     }
    
//     webSocket.loop();
    
//     // Add timing control for consistent sampling rate
//     static unsigned long lastSampleTime = 0;
//     unsigned long currentTime = millis();
//     if (currentTime - lastSampleTime < 20) {  // 50Hz sampling rate
//         return;
//     }
//     lastSampleTime = currentTime;
    
//     if (mpu.dmpGetCurrentFIFOPacket(FIFOBuffer)) {
//         mpu.dmpGetQuaternion(&q, FIFOBuffer);
//         mpu.dmpGetGravity(&gravity, &q);
//         mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
        
//         StaticJsonDocument<200> doc;
//         doc["yaw"] = ypr[0] * 180/M_PI;
//         doc["pitch"] = ypr[1] * 180/M_PI;
//         doc["roll"] = ypr[2] * 180/M_PI;
        
//         String jsonString;
//         serializeJson(doc, jsonString);
        
//         webSocket.broadcastTXT(jsonString);
//     }
// }
