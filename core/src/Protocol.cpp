#include "rb/Protocol.hpp"

namespace rb {
namespace {
void stringCapacity(const std::string &value, size_t max, const char *field) {
    if (value.size() > max || value.find('\0') != std::string::npos)
        throw Error(std::string("invalid original protocol string: ") + field);
}
Property str(const char *name, const std::string &value) {
    Property p;
    p.name = name;
    p.type = 11;
    p.value.assign(value.begin(), value.end());
    p.value.push_back(0);
    return p;
}
Property s16(const char *name, int16_t value) {
    Property p;
    p.name = name;
    p.type = 4;
    p.value = {uint8_t(uint16_t(value) >> 8), uint8_t(value)};
    return p;
}
} // namespace
const ProtocolMethod &playerReadMethod() {
    static const ProtocolMethod method{"local2", "player", "rb6_player_read", 0x1009CEE0};
    return method;
}
const ProtocolMethod &playerReadScoreMethod() {
    static const ProtocolMethod method{"local2", "player", "rb6_player_read_score", 0x1009C430};
    return method;
}
PlayerReadRequest PlayerReadRequest::prepare(std::string rid, std::string lid, int16_t version,
                                             std::string cardID, int32_t cardType) {
    stringCapacity(rid, 16, "rid");
    stringCapacity(lid, 8, "lid");
    stringCapacity(cardID, 16, "card_id");
    return {std::move(rid), std::move(lid), std::move(cardID), version,
            int16_t(cardType == 1   ? 1
                    : cardType == 2 ? 2
                                    : 0)};
}
std::vector<Property> PlayerReadRequest::fields() const {
    stringCapacity(referenceID, 16, "rid");
    stringCapacity(locationID, 8, "lid");
    stringCapacity(cardID, 16, "card_id");
    if (cardType < 0 || cardType > 2)
        throw Error("invalid prepared card type");
    return {str("rid", referenceID), str("lid", locationID), s16("ver", version),
            str("card_id", cardID), s16("card_type", cardType)};
}
std::vector<Property> playerReadScoreFields(const std::string &referenceID) {
    stringCapacity(referenceID, 16, "rid");
    return {str("rid", referenceID)};
}
int32_t XrpcCallbackState::originalStatus() const {
    if (pending)
        return 3;
    if (callback2) {
        switch (callback2) {
        case 100:
        case 101:
            return 2;
        case 102:
            return 14;
        case 109:
            return 6;
        case 110:
            return 7;
        case 111:
            return 15;
        case 112:
            return 9;
        case 113:
            return 10;
        case 114:
            return 11;
        case 115:
            return 13;
        case 116:
            return 5;
        case 120:
            return 12;
        case 123:
            return 8;
        default:
            return 1;
        }
    }
    if (callback0)
        return 0;
    return callback3 ? 1 : 4;
}
} // namespace rb
