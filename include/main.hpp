#ifndef MAIN_HPP
#define MAIN_HPP

// -----------------------------------------------
//
// main.hpp for FlowSensorTest
//
// LilyGo T-Display S3 (ESP32-S3) + ST7789 170x320
// Display library: TFT_eSPI (bodmer)
//
// -----------------------------------------------

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include "SFM3200.hpp"
#include "FlowDisplay.hpp"

#define hostCom Serial

#define LCD_WIDTH  320
#define LCD_HEIGHT 170

// I2C pins for the SFM3200. Change to match wiring.
#define SFM_SDA_PIN 43   // green cable
#define SFM_SCL_PIN 44   // blue cable
#define SFM_I2C_HZ  400000

#define UPDATE_INTERVAL_MS 50

#endif // MAIN_HPP
