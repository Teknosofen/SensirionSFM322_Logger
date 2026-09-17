#include "FlowDisplay.hpp"

namespace {
constexpr int16_t MARGIN_X      = 8;
constexpr int16_t HEADER_Y      = 4;
constexpr int16_t HEADER_H      = 28;
constexpr int16_t VALUE_Y       = 44;   // top of the big value glyph row
constexpr int16_t UNIT_GAP      = 8;    // gap between value and inline unit
constexpr int16_t RAW_GAP       = 6;    // gap between value bottom and raw line
constexpr uint8_t HEADER_FONT   = 4;
constexpr uint8_t VALUE_FONT    = 7;
constexpr uint8_t UNIT_FONT     = 4;
constexpr uint8_t RAW_FONT      = 2;

constexpr uint16_t BG_COLOR      = TFT_MIDNIGHTBLUE;
constexpr uint16_t HEADER_COLOR  = TFT_LOGOBACKGROUND;
constexpr uint16_t DIVIDER_COLOR = TFT_SLATEBLUE;
constexpr uint16_t VALUE_COLOR   = TFT_GREENISH_TINT;
constexpr uint16_t UNIT_COLOR    = TFT_LOGOBACKGROUND;
constexpr uint16_t RAW_COLOR     = TFT_SLATEBLUE;
constexpr uint16_t ERROR_COLOR   = TFT_REDDISH_TINT;
}

FlowDisplay::FlowDisplay(TFT_eSPI& tft, const String& header)
    : tft_(tft), header_(header) {}

void FlowDisplay::begin() {
    clear();
    drawHeader();
}

void FlowDisplay::setHeader(const String& header) {
    header_ = header;
    drawHeader();
}

void FlowDisplay::clear() {
    tft_.fillScreen(BG_COLOR);
    lastHeader_   = "";
    lastValueStr_ = "";
    lastUnit_     = "";
    lastRawStr_   = "";
    lastError_    = "";
    lastUnitX_    = 0;
    lastWasError_ = false;
}

void FlowDisplay::drawHeader() {
    tft_.setTextDatum(TL_DATUM);
    if (lastHeader_ != header_ && lastHeader_.length()) {
        tft_.setTextColor(BG_COLOR, BG_COLOR);
        tft_.drawString(lastHeader_, MARGIN_X, HEADER_Y, HEADER_FONT);
    }
    tft_.setTextColor(HEADER_COLOR, BG_COLOR);
    tft_.drawString(header_, MARGIN_X, HEADER_Y, HEADER_FONT);
    tft_.drawFastHLine(MARGIN_X, HEADER_H + 6, tft_.width() - 2 * MARGIN_X, DIVIDER_COLOR);
    lastHeader_ = header_;
}

void FlowDisplay::showFlow(float value, uint16_t raw, const char* unit, uint8_t decimals) {
    const String valueStr = String(value, static_cast<unsigned int>(decimals));
    const String unitStr  = unit ? String(unit) : String("");
    const String rawStr   = String("raw=") + raw;

    const int16_t valueH = tft_.fontHeight(VALUE_FONT);
    const int16_t unitH  = tft_.fontHeight(UNIT_FONT);
    const int16_t rawY   = VALUE_Y + valueH + RAW_GAP;

    tft_.setTextDatum(TL_DATUM);

    if (lastWasError_ && lastError_.length()) {
        tft_.setTextColor(BG_COLOR, BG_COLOR);
        tft_.drawString(lastError_, MARGIN_X, VALUE_Y, HEADER_FONT);
        lastError_ = "";
    }

    const bool valueChanged = (valueStr != lastValueStr_);
    if (valueChanged) {
        if (lastValueStr_.length()) {
            tft_.setTextColor(BG_COLOR, BG_COLOR);
            tft_.drawString(lastValueStr_, MARGIN_X, VALUE_Y, VALUE_FONT);
        }
        tft_.setTextColor(VALUE_COLOR, BG_COLOR);
        tft_.drawString(valueStr, MARGIN_X, VALUE_Y, VALUE_FONT);
        lastValueStr_ = valueStr;
    }

    const int16_t valueW = tft_.textWidth(valueStr, VALUE_FONT);
    const int16_t unitX  = MARGIN_X + valueW + UNIT_GAP;
    const int16_t unitY  = VALUE_Y + valueH - unitH - 4;   // baseline-align to value

    if (valueChanged || unitStr != lastUnit_) {
        if (lastUnit_.length()) {
            tft_.setTextColor(BG_COLOR, BG_COLOR);
            tft_.drawString(lastUnit_, lastUnitX_, unitY, UNIT_FONT);
        }
        tft_.setTextColor(UNIT_COLOR, BG_COLOR);
        tft_.drawString(unitStr, unitX, unitY, UNIT_FONT);
        lastUnit_  = unitStr;
        lastUnitX_ = unitX;
    }

    if (rawStr != lastRawStr_) {
        if (lastRawStr_.length()) {
            tft_.setTextColor(BG_COLOR, BG_COLOR);
            tft_.drawString(lastRawStr_, MARGIN_X, rawY, RAW_FONT);
        }
        tft_.setTextColor(RAW_COLOR, BG_COLOR);
        tft_.drawString(rawStr, MARGIN_X, rawY, RAW_FONT);
        lastRawStr_ = rawStr;
    }

    lastWasError_ = false;
}

void FlowDisplay::showError(const String& message) {
    const int16_t valueH = tft_.fontHeight(VALUE_FONT);
    const int16_t rawY   = VALUE_Y + valueH + RAW_GAP;
    const int16_t unitH  = tft_.fontHeight(UNIT_FONT);
    const int16_t unitY  = VALUE_Y + valueH - unitH - 4;

    tft_.setTextDatum(TL_DATUM);
    tft_.setTextColor(BG_COLOR, BG_COLOR);
    if (lastValueStr_.length()) {
        tft_.drawString(lastValueStr_, MARGIN_X, VALUE_Y, VALUE_FONT);
        lastValueStr_ = "";
    }
    if (lastUnit_.length()) {
        tft_.drawString(lastUnit_, lastUnitX_, unitY, UNIT_FONT);
        lastUnit_ = "";
    }
    if (lastRawStr_.length()) {
        tft_.drawString(lastRawStr_, MARGIN_X, rawY, RAW_FONT);
        lastRawStr_ = "";
    }
    if (lastWasError_ && lastError_.length() && lastError_ != message) {
        tft_.drawString(lastError_, MARGIN_X, VALUE_Y, HEADER_FONT);
    }

    tft_.setTextColor(ERROR_COLOR, BG_COLOR);
    tft_.drawString(message, MARGIN_X, VALUE_Y, HEADER_FONT);

    lastError_    = message;
    lastWasError_ = true;
}
