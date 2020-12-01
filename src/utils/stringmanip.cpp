//
// Created by jglrxavpok on 01/12/2020.
//

#include "stringmanip.h"

vector<string> Carrot::splitString(const string& toSplit, const string& delimiter) {
    vector<string> parts{};

    string remaining = toSplit;
    while(!remaining.empty()) {
        auto nextDelimiter = remaining.find(delimiter);
        parts.push_back(remaining.substr(0, nextDelimiter));
        if(nextDelimiter == string::npos) {
            remaining = "";
        } else {
            remaining = remaining.substr(nextDelimiter+1);
        }
    }
    return parts;
}