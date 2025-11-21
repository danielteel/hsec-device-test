#pragma once
#include <Arduino.h>

class DanParser {
public:
    DanParser();

    void setInputText(const String &text);

    bool hadError() const;

    bool getName(String &out);
    bool match(char c);
    bool getText(String &out);   
    bool getDouble(double &out);      // parse floating point
    bool getInt32(int32_t &out);      // parse 32-bit integer

private:
    String input;
    size_t index;
    char look;
    bool hasError;

    void error(const char* msg);

    void getChar();
    bool isAlphaNumeric(char c);
    bool isWhite(char c);
    void skipWhite();
    
};