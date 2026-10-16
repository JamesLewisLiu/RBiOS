#include "rb/Protocol.hpp"
#include <functional>
#include <iostream>
using namespace rb;
static int checks = 0;
static void check(bool b) {
    ++checks;
    if (!b)
        throw Error("protocol check " + std::to_string(checks));
}
static void rejects(const std::function<void()> &f) {
    try {
        f();
    } catch (const Error &) {
        ++checks;
        return;
    }
    throw Error("expected protocol rejection");
}
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--status") {
            XrpcCallbackState s;
            while (std::cin >> s.callback0 >> s.callback2 >> s.callback3 >> s.pending)
                std::cout << s.originalStatus() << '\n';
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--read-fields") {
            auto q = PlayerReadRequest::prepare("0123456789ABCDEF", "ABCD1234", -32768,
                                                "FEDCBA9876543210", std::stoi(argv[2]));
            PropertyDocument d;
            d.root.name = "synthetic";
            d.root.children = q.fields();
            // Hex wire output avoids Windows text/binary translation.
            constexpr char hex[] = "0123456789abcdef";
            for (auto b : d.encode())
                std::cout << hex[b >> 4] << hex[b & 15];
            std::cout << '\n';
            return 0;
        }
        for (int input : {-1, 0, 1, 2, 3, 999}) {
            const auto q =
                PlayerReadRequest::prepare("0123456789ABCDEF", "ABCD1234", 12, "", input);
            check(q.cardType == (input == 1 ? 1 : input == 2 ? 2 : 0));
            const auto fields = q.fields();
            check(fields.size() == 5);
            check(fields[0].name == "rid" && fields[1].name == "lid" && fields[2].name == "ver");
            check(fields[2].type == 4 && fields[2].integers()[0] == 12);
            check(fields[3].name == "card_id" && fields[3].value == Bytes{0});
            check(fields[4].integers()[0] == q.cardType);
            PropertyDocument d;
            d.root.name = "synthetic";
            d.root.children = fields;
            auto decoded = PropertyDocument::decode(d.encode());
            check(decoded.root.children[0].string() == q.referenceID);
            check(decoded.root.children[4].integers()[0] == q.cardType);
        }
        rejects([] { PlayerReadRequest::prepare(std::string(17, 'A'), "", 0, "", 0); });
        rejects([] { PlayerReadRequest::prepare("", std::string(9, 'A'), 0, "", 0); });
        rejects([] { PlayerReadRequest::prepare("", "", 0, std::string(17, 'A'), 0); });
        rejects([] { PlayerReadRequest::prepare(std::string("A\0B", 3), "", 0, "", 0); });
        const auto score = playerReadScoreFields("0123456789ABCDEF");
        check(score.size() == 1 && score[0].name == "rid" && score[0].type == 11);
        check(std::string(playerReadMethod().method) == "rb6_player_read");
        check(std::string(playerReadScoreMethod().service) == "local2");
        check(XrpcCallbackState{}.originalStatus() == 4);
        check(XrpcCallbackState{1, 0, 0, false}.originalStatus() == 0);
        check(XrpcCallbackState{0, 100, 0, false}.originalStatus() == 2);
        check(XrpcCallbackState{0, 123, 0, true}.originalStatus() == 3);
        std::cout << "PASS " << checks << " protocol assertions (no transport)\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
