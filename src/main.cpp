#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP4728.h>

Adafruit_MCP4728 mcp;

void setup(void) {
  Serial.begin(115200);
  while (!Serial)
    delay(10); // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("Adafruit MCP4728 test!");

  // Try to initialize!
  if (!mcp.begin(0x64)) {
    Serial.println("Failed to find MCP4728 chip");
    while (1) {
      delay(10);
    }
  }
  mcp.setChannelValue(MCP4728_CHANNEL_A, 4095);
  mcp.setChannelValue(MCP4728_CHANNEL_B, 2048);
  mcp.setChannelValue(MCP4728_CHANNEL_C, 1024);
  mcp.setChannelValue(MCP4728_CHANNEL_D, 0);
}

void loop() {  
  // loop a up/down ramp from 0-4095 and back with no delay
  for (uint16_t i = 0; i < 4096; i++) {
    mcp.setChannelValue(MCP4728_CHANNEL_A, i);
    mcp.setChannelValue(MCP4728_CHANNEL_B, i);
    mcp.setChannelValue(MCP4728_CHANNEL_C, i);
    mcp.setChannelValue(MCP4728_CHANNEL_D, i);
  }
  for (uint16_t i = 4095; i > 0; i--) {
    mcp.setChannelValue(MCP4728_CHANNEL_A, i);
    mcp.setChannelValue(MCP4728_CHANNEL_B, i);
    mcp.setChanne>lValue(MCP4728_CHANNEL_C, i);
    mcp.setChannelValue(MCP4728_CHANNEL_D, i);
  }
}
