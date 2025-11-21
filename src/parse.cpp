#include "parse.h"

DanParser::DanParser() : input(""), index(0), look(0), hasError(false) {}

void DanParser::setInputText(const String &text) {
    input = text;
    index = 0;
    hasError = false;
    look = (input.length() > 0 ? input[0] : '\0');
}

bool DanParser::hadError() const {
    return hasError;
}

bool DanParser::getName(String &out) {
    if (hasError) return false;

    skipWhite();
    out = "";

    while (look && (isAlphaNumeric(look) || isWhite(look))) {
        out += look;
        getChar();
    }

    out.trim();
    out.toLowerCase();

    return true;
}

bool DanParser::match(char c) {
    if (hasError) return false;

    skipWhite();
    if (look != c) {
        error("Match failure");
        return false;
    }

    getChar();
    return true;
}

bool DanParser::getText(String &out) {
    if (hasError) return false;

    out = "";

    while (look != '\0') {
        out += look;
        getChar();
    }
    return true;
}

bool DanParser::getDouble(double &out) {
    if (hasError) return false;

    skipWhite();
    String numStr = "";

    // optional minus
    if (look == '-') {
        numStr += look;
        getChar();
    }

    bool hasDigits = false;
    bool hasDot = false;

    while (look) {
        if (look >= '0' && look <= '9') {
            numStr += look;
            hasDigits = true;
        } else if (look == '.' && !hasDot) {
            numStr += look;
            hasDot = true;
        } else {
            break;
        }
        getChar();
    }

    if (!hasDigits) {
        error("Invalid double");
        return false;
    }

    out = numStr.toDouble();
    return true;
}


bool DanParser::getInt32(int32_t &out) {
    if (hasError) return false;

    skipWhite();
    String numStr = "";

    // optional minus
    if (look == '-') {
        numStr += look;
        getChar();
    }

    bool hasDigits = false;

    while (look && (look >= '0' && look <= '9')) {
        numStr += look;
        hasDigits = true;
        getChar();
    }

    if (!hasDigits) {
        error("Invalid int32");
        return false;
    }

    out = numStr.toInt();  // Arduino String method returns int
    return true;
}


// --------------------- Internal helpers ---------------------

void DanParser::error(const char* msg) {
    hasError = true;
    Serial.print("DanParser error: ");
    Serial.println(msg);
}

void DanParser::getChar() {
    if (hasError) return;

    index++;
    if (index < input.length()) look = input[index];
    else look = '\0';
}

bool DanParser::isAlphaNumeric(char c) {
    return ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9'));
}

bool DanParser::isWhite(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

void DanParser::skipWhite() {
    while (isWhite(look)) getChar();
}