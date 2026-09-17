#ifndef FLOWDISPLAY_HPP
#define FLOWDISPLAY_HPP

// -----------------------------------------------
//
// FlowDisplay - small UI helper for LilyGo T-Display S3
//
// Renders a header line and a large flow reading with unit.
//
// -----------------------------------------------

#include <Arduino.h>
#include <TFT_eSPI.h>

// Colours borrowed from LundaLogger (RGB565, byte-flipped as used there).
#define TFT_LOGOBACKGROUND   0x85BA
#define TFT_LOGOBLUE         0x5497
#define TFT_DARKERBLUE       0x3A97
#define TFT_DEEPBLUE         0x1A6F
#define TFT_SLATEBLUE        0x2B4F
#define TFT_MIDNIGHTBLUE     0x1028
#define TFT_REDDISH_TINT     0xA4B2
#define TFT_GREENISH_TINT    0x5DAD

class FlowDisplay {
public:
    explicit FlowDisplay(TFT_eSPI& tft, const String& header = "IaT-team");

    void begin();
    void setHeader(const String& header);

    // Redraw header + flow value with unit and small raw counts line.
    void showFlow(float value, uint16_t raw, const char* unit = "slm", uint8_t decimals = 2);

    // Show an error message in the flow area.
    void showError(const String& message);

    void clear();

private:
    TFT_eSPI& tft_;
    String    header_;
    String    lastHeader_;
    String    lastValueStr_;
    String    lastUnit_;
    String    lastRawStr_;
    String    lastError_;
    int16_t   lastUnitX_    = 0;
    bool      lastWasError_ = false;

    void drawHeader();
};

#endif // FLOWDISPLAY_HPP
