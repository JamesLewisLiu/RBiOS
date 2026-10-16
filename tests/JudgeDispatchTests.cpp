#include "rb/Binary.hpp"
#include "rb/JudgeDispatch.hpp"
#include "rb/Score.hpp"
#include <iostream>
using namespace rb;
int main(int argc, char **) {
    try {
        if (argc > 1) {
            DispatchNote n;
            int ended, completed, held, released, cursor;
            while (std::cin >> n.kind >> n.relativeTick >> n.durationTicks >> n.segmentElapsed >>
                   n.segmentIndex >> n.segmentDuration >> ended >> completed >> held >> n.style >>
                   n.contactId >> n.switchCount >> n.switchDirection >> released >> cursor) {
                n.ended = ended;
                n.completed = completed;
                n.held = held;
                n.touchReleased = released;
                n.replayCursor = int8_t(cursor);
                int mode, miss, press, hold, sp, sh, replayCount;
                unsigned table;
                bool reject;
                std::cin >> mode >> table >> reject >> miss >> press >> hold >> sp >> sh >>
                    replayCount;
                std::vector<uint8_t> codes;
                for (int i = 0; i < replayCount; ++i) {
                    int c;
                    std::cin >> c;
                    codes.push_back(uint8_t(c));
                }
                JudgmentQueries q;
                q.missed = [&] { return miss != 0; };
                q.pressed = [&] { return press != 0; };
                q.holdCovered = [&] { return hold != 0; };
                q.switchHeld = [&] { return sh != 0; };
                q.switchPress = [&]() -> std::optional<int32_t> {
                    if (sp < 0)
                        return {};
                    return sp;
                };
                if (replayCount >= 0)
                    q.replay = &codes;
                auto r = dispatchJudgment(n, mode, table, reject, q);
                std::cout << bool(r) << ' ' << (r ? r->grade : 6) << ' ' << (r ? r->reason : -1)
                          << ' ' << n.held << ' ' << n.contactId << '\n';
            }
            return 0;
        }
        unsigned count = 0;
        auto check = [&](bool ok) {
            ++count;
            if (!ok)
                throw Error("dispatch assertion " + std::to_string(count));
        };
        DispatchNote n;
        JudgmentQueries q;
        q.missed = [] { return false; };
        q.pressed = [] { return true; };
        q.holdCovered = [] { return true; };
        q.switchPress = []() -> std::optional<int32_t> { return {}; };
        q.switchHeld = [] { return true; };
        n.relativeTick = 0;
        auto r = dispatchJudgment(n, 0, 0, false, q);
        check(r && r->grade == 0 && r->reason == 0);
        BattleScore b;
        PlayerScore p;
        p.totalEvents = 1;
        b.baseCount[0] = 1;
        b.life[0] = 100;
        b.lifeDelta[0] = 10;
        submitScore(
            b, p,
            ScoreEvent{0, unsigned(r->grade), r->reason, n.kind, n.relativeTick, false, false},
            true, false);
        check(b.score[0] == 6 && p.maxCombo == 1 && p.finalRate() == 1);
        n.kind = 1;
        n.durationTicks = 60;
        r = dispatchJudgment(n, 0, 0, false, q);
        check(r && r->grade == 4 && r->reason == 7 && n.held);
        n.relativeTick = -15;
        r = dispatchJudgment(n, 0, 0, false, q);
        check(r && r->grade == 3 && r->reason == 4 && n.held);
        q.holdCovered = [] { return false; };
        r = dispatchJudgment(n, 0, 0, false, q);
        check(r && r->grade == 3 && r->reason == 8 && !n.held && n.contactId == -1);
        n = DispatchNote{};
        n.kind = 4;
        n.segmentDuration = 30;
        n.segmentElapsed = 15;
        n.segmentIndex = 0;
        r = dispatchJudgment(n, 1, 0, false, q);
        check(r && r->grade == 3 && r->reason == 4 && n.held);
        std::vector<uint8_t> replay{2};
        q.replay = &replay;
        n = DispatchNote{};
        r = dispatchJudgment(n, 3, 0, false, q);
        check(r && r->grade == 1 && r->reason == 11);
        q.replay = nullptr;
        n.kind = 0;
        n.relativeTick = 10;
        check(!dispatchJudgment(n, 3, 0, false, q));
        q.missed = {};
        bool threw = false;
        try {
            dispatchJudgment(n, 0, 0, false, q);
        } catch (const Error &) {
            threw = true;
        }
        check(threw);
        std::cout << "PASS " << count << " dispatch assertions\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
