#include "ScoreWire.hpp"
#include "rb/Gameplay.hpp"
#include <iostream>
using namespace rb;
int main(int argc, char **) {
    try {
        if (argc > 1) {
            char op;
            while (std::cin >> op) {
                RuntimeNote n;
                BattleScore b;
                PlayerScore p;
                StageScoreState stage;
                readWire(b, p);
                int cursorValue, ended, success, blocked, completed, held, penalty, special,
                    disabled, mode;
                unsigned diff, gauge;
                int option;
                JudgmentResult r;
                std::cin >> n.judgment.kind >> n.judgment.relativeTick >>
                    n.judgment.durationTicks >> n.flags >> cursorValue >> ended >> success >>
                    blocked >> n.touch.cooldown >> n.deviation >> n.judgment.style >>
                    n.judgment.segmentIndex >> n.judgment.segmentElapsed >>
                    n.judgment.segmentDuration >> n.judgment.switchDirection >> completed >> held >>
                    n.judgment.contactId >> n.side >> penalty >> special >> r.grade >> r.reason >>
                    disabled >> mode >> diff >> gauge >> option;
                n.judgment.replayCursor = int8_t(cursorValue);
                n.judgment.ended = ended;
                n.successful = success;
                n.touch.inputBlocked = blocked;
                n.judgment.completed = completed;
                n.judgment.held = held;
                n.specialPenalty = penalty;
                n.special = special;
                unsigned count;
                std::cin >> count;
                for (unsigned i = 0; i < count; ++i) {
                    int duration;
                    std::cin >> duration;
                    n.switchDurations.push_back(duration);
                }
                n.judgment.switchCount = int32_t(count);
                stage.scoringDisabled = disabled;
                stage.stageModeSix = mode;
                stage.difficulty = diff;
                stage.gaugeMode = gauge;
                stage.clearOption = option;
                consumeJudgment(n, r, b, p, stage);
                // Full score/stat wire, followed by all migrated note/counter fields.
                transfer(b, p, false);
                constexpr char h[] = "0123456789abcdef";
                for (auto v : wire)
                    std::cout << h[v >> 4] << h[v & 15];
                std::cout << ' ' << int(n.judgment.replayCursor) << ' ' << n.judgment.ended << ' '
                          << n.successful << ' ' << n.touch.inputBlocked << ' ' << n.touch.cooldown
                          << ' ' << n.deviation << ' ' << n.judgment.style << ' '
                          << n.judgment.segmentIndex << ' ' << n.judgment.segmentElapsed << ' '
                          << n.judgment.segmentDuration << ' ' << n.judgment.switchDirection << ' '
                          << n.judgment.completed << ' ' << n.judgment.held << ' '
                          << n.judgment.contactId << ' ' << stage.processed << ' ' << stage.dirty
                          << '\n';
                if (!std::cin)
                    throw Error("incomplete gameplay fixture");
            }
            return 0;
        }
        unsigned checks = 0;
        auto check = [&](bool ok) {
            ++checks;
            if (!ok)
                throw Error("gameplay assertion " + std::to_string(checks));
        };
        RuntimeNote n;
        n.touch.region = 1;
        n.touch.x = 384;
        n.touch.y = 1100;
        n.touch.windowStart = -9;
        n.touch.windowEnd = 9;
        TouchState touch;
        touch.contacts[0] = {true, 42, 384, 1100, 3, false, false, false, 0};
        BattleScore b;
        PlayerScore p;
        StageScoreState stage;
        InputRules rules;
        p.totalEvents = 1;
        b.baseCount[0] = 1;
        b.life[0] = 100;
        b.lifeDelta[0] = 10;
        auto result = evaluateNote(n, touch, rules, b, p, stage);
        check(result && result->scored && result->normalized.grade == 0 && n.judgment.ended);
        check(b.score[0] == 56 && p.maxCombo == 1 && p.finalRate() == 1 && result->medal == 17);
        check(n.judgment.contactId == 42 && touch.contacts[0].selected &&
              touch.contacts[0].reuseDelay == 40);
        n = RuntimeNote{};
        n.judgment.kind = 1;
        n.judgment.durationTicks = 60;
        n.touch.region = 1;
        n.touch.x = 384;
        n.touch.y = 1100;
        n.touch.windowEnd = 9;
        n.touch.phase = 30;
        touch.contacts[0].selected = false;
        p.reset();
        p.totalEvents = 5;
        b.reset();
        b.baseCount[0] = 5;
        stage = {};
        result = evaluateNote(n, touch, rules, b, p, stage);
        check(result && !result->counted && !result->scored && n.judgment.held && p.counts[4] == 0);
        n.judgment.relativeTick = -15;
        result = evaluateNote(n, touch, rules, b, p, stage);
        check(result && result->normalized.reason == 4 && b.score[0] == 1 && p.counts[3] == 1 &&
              n.judgment.replayCursor == 0);
        touch.contacts[0].active = false;
        for (int i = 0; i < 5; ++i)
            check(!evaluateNote(n, touch, rules, b, p, stage) || n.judgment.held);
        result = evaluateNote(n, touch, rules, b, p, stage);
        check(result && !n.judgment.held && result->normalized.reason == 4);
        n = RuntimeNote{};
        n.judgment.kind = 4;
        n.judgment.segmentIndex = -1;
        n.judgment.switchCount = 2;
        n.judgment.segmentDuration = 0;
        n.switchDurations = {30, 60};
        applyBaseNoteEvent(n, {0, 5});
        check(n.judgment.segmentIndex == 0 && n.judgment.segmentDuration == 30 &&
              !n.judgment.ended);
        n.judgment.segmentElapsed = 30;
        applyBaseNoteEvent(n, {0, 5});
        check(n.judgment.segmentIndex == 1 && n.judgment.segmentElapsed == 0 &&
              n.judgment.segmentDuration == 60);
        n.judgment.segmentElapsed = 60;
        applyBaseNoteEvent(n, {0, 5});
        check(n.judgment.completed && n.judgment.ended);
        RuntimeNote next;
        n = RuntimeNote{};
        n.next = &next;
        next.previous = &n;
        n.flags = 8;
        applyBaseNoteEvent(n, {0, 0});
        check(n.touch.cooldown == 2 && next.touch.cooldown == 2 && next.touch.inputBlocked &&
              !next.previous);
        n = RuntimeNote{};
        n.chartIndex = 0;
        n.side = 1;
        n.touch.region = 1;
        rules.mode = 3;
        CPUState cpu;
        cpu.entries.push_back({0, 0, 0, 0});
        cpu.specialEnabled = false;
        auto random = OriginalRandom::seeded(1);
        p.reset();
        p.totalEvents = 1;
        stage = {};
        b.reset();
        b.baseCount[1] = 1;
        result = evaluateNote(n, touch, rules, b, p, stage, &cpu, &random);
        check(result && result->scored && p.counts[0] == 1 && b.score[1] == 56 &&
              cpu.entries[0].consumed);
        n = RuntimeNote{};
        n.chartIndex = 0;
        n.side = 1;
        n.touch.region = 1;
        n.flags = 0x20;
        cpu = CPUState{};
        cpu.entries.push_back({0, 0, 0, 0});
        cpu.difficulty = 17;
        p.reset();
        p.justReflec = 100;
        stage = {};
        b.reset();
        b.baseCount[1] = 1;
        random = OriginalRandom::seeded(1);
        result = evaluateNote(n, touch, rules, b, p, stage, &cpu, &random);
        check(result && result->original.reason == 12 && cpu.meter == 100);
        std::vector<RuntimeNote> chain(300);
        for (size_t i = 1; i < chain.size(); ++i)
            chain[i - 1].next = &chain[i];
        chain[0].flags = 8;
        applyBaseNoteEvent(chain[0], {0, 0});
        check(chain.back().touch.cooldown == 2);
        n = RuntimeNote{};
        n.judgment.relativeTick = -1;
        n.touch.y = 1300;
        n.touch.x = 384;
        rules.mode = 0;
        p.reset();
        stage = {};
        b.reset();
        result = evaluateNote(n, touch, rules, b, p, stage);
        check(result && result->original.grade == 4 && n.judgment.ended);
        std::cout << "PASS " << checks << " gameplay assertions\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
