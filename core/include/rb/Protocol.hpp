#pragma once
#include "Property.hpp"

namespace rb {
struct ProtocolMethod {
    const char *service;
    const char *module;
    const char *method;
    uint32_t fieldBuilder;
};
const ProtocolMethod &playerReadMethod();
const ProtocolMethod &playerReadScoreMethod();

// Request fields only. EA3's call envelope and transport options are not inferred.
// Strings are original encoding bytes, not UTF-8 conversions.
struct PlayerReadRequest {
    std::string referenceID, locationID, cardID;
    int16_t version = 0, cardType = 0;
    // 0x1009CFE0 mapping. Out-of-capacity strings are rejected by the adapter;
    // original AVS string-copy truncation is deliberately not guessed.
    static PlayerReadRequest prepare(std::string rid, std::string lid, int16_t version,
                                     std::string cardID, int32_t inputCardType);
    std::vector<Property> fields() const; // 0x1009CEE0 field order/types
};
std::vector<Property> playerReadScoreFields(const std::string &referenceID); // 0x1009C430

// Selected fields of the original +520 callback state. Keep numeric meanings
// until EA3's error constants are recovered; this is not an HTTP status map.
struct XrpcCallbackState {
    int32_t callback0 = 0;          // +520
    int32_t callback2 = 0;          // +528
    int32_t callback3 = 0;          // +532
    bool pending = false;           // +540
    int32_t originalStatus() const; // 0x1009F1B0
};
} // namespace rb
