#include "ScoreWire.hpp"
int main(int argc, char **) {
    try {
        if (argc > 1) {
            char op;
            while (std::cin >> op) {
                if (op == 'L') {
                    int l, n;
                    unsigned g;
                    std::cin >> l >> g >> n;
                    std::cout << lifeIncrement(l, g, n) << '\n';
                    continue;
                }
                BattleScore b;
                PlayerScore p;
                readWire(b, p);
                if (op == 'E') {
                    ScoreEvent e;
                    bool remote, mode;
                    std::cin >> e.reason >> e.grade >> e.side >> e.kind >> e.deviation >>
                        e.specialNote >> e.specialPenalty >> remote >> mode;
                    submitScore(b, p, e, remote, mode);
                    writeWire(b, p);
                } else if (op == 'F') {
                    unsigned i, s;
                    int r, m;
                    bool mode;
                    std::cin >> i >> s >> r >> m >> mode;
                    b.applyEffect(i, s, r, m, mode);
                    writeWire(b, p);
                } else if (op == 'A') {
                    unsigned d;
                    int option;
                    std::cin >> d >> option;
                    std::cout << bits(p.projectedRate()) << ' ' << bits(p.currentRate()) << ' '
                              << bits(p.finalRate()) << ' ' << p.performanceScore(d) << ' '
                              << p.clearType(d, option) << ' ' << p.medal(d, option) << '\n';
                } else if (op == 'R') {
                    b.reset();
                    p.reset();
                    writeWire(b, p);
                } else if (op == 'C') {
                    p.consumeJustReflec();
                    writeWire(b, p);
                } else
                    throw Error("unknown score fixture");
                if (!std::cin)
                    throw Error("incomplete score fixture");
            }
            return 0;
        }
        unsigned checks = 0;
        auto check = [&](bool ok) {
            ++checks;
            if (!ok)
                throw Error("score assertion " + std::to_string(checks));
        };
        BattleScore b;
        PlayerScore p;
        b.maxGauge = {10000, 10000, 5000, 100, 100};
        b.gauge = {5000, 5000, 2500, 50, 50};
        b.scale = {1, 0, 1, 1};
        b.life = {100, 100};
        b.lifeDelta = {10, 5, -5, 1, -200};
        b.baseCount = {10, 10};
        p.totalEvents = 10;
        ScoreEvent e;
        for (int i = 0; i < 10; ++i)
            submitScore(b, p, e, true, false);
        check(b.score[0] == 60 && p.combo == 10 && p.maxCombo == 10 && p.justReflec == 50);
        check(p.finalRate() == 1 && p.currentRate() == 1 && p.projectedRate() == 1);
        check(p.performanceScore(0) == 10000 && p.medal(0, 0) == 0x11);
        e.grade = 4;
        submitScore(b, p, e, true, false);
        check(p.combo == 0 && p.maxCombo == 10 && !p.fullCombo && b.life[0] == 0);
        check(p.timingHistogram[0] == 1 && p.timingHistogram[4] == 10);
        e.grade = 0;
        submitScore(b, p, e, true, false);
        check(b.life[0] == 0);
        b.gauge = {100, 100, 100, 1, 50};
        e.grade = 4;
        e.reason = 4;
        int jr = p.justReflec;
        submitScore(b, p, e, true, false);
        check(b.gauge[0] == 1 && b.gauge[1] == 1 && b.gauge[3] == 1 && p.justReflec == jr);
        e.side = 1;
        e.grade = 0;
        int combo = p.combo;
        submitScore(b, p, e, false, false);
        check(p.combo == combo);
        p.justReflec = 99;
        p.consumeJustReflec();
        check(p.justReflec == 0 && p.usedJustReflec == 1);
        p.initialStreak = 7;
        p.reset();
        check(p.streak == 7 && p.totalEvents == 10 && p.fullCombo);
        b.score[0] = 9998;
        b.applyEffect(0, 0, 0, 0, false);
        check(b.score[0] == 9999);
        check(lifeIncrement(10, 4, 1000) == -200 && lifeIncrement(10, 3, 1000) == 16);
        p.overrideRate = true;
        p.overriddenRate = .75f;
        check(p.finalRate() == .75f && p.performanceScore(2) == 7000);
        p.retired = true;
        check(p.clearType(2, 1) == 1 && p.medal(2, 1) == 0);
        std::cout << "PASS " << checks << " score assertions\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
