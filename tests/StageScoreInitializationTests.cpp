#include "rb/StageScoreInitialization.hpp"
#include "ScoreWire.hpp"
int main(int argc, char **) {
    try {
        if (argc > 1) {
            StageScoreInitialization in;
            while (std::cin >> in.selectedChartSide >> in.ruleFlags >> in.removeLifeFloor >> in.lifeLevel) {
                for (auto &v : in.total) std::cin >> v;
                for (auto &v : in.longTicks) std::cin >> v;
                for (auto &v : in.afterJustReflec) std::cin >> v;
                for (auto &v : in.gauge) std::cin >> v;
                for (auto &v : in.maximum) std::cin >> v;
                for (auto &v : in.scale) std::cin >> v;
                if (!std::cin) throw Error("truncated stage score fixture");
                auto b = initializeStageScore(in); PlayerScore p;
                std::fill(wire.begin(),wire.end(),0); writeWire(b,p);
            }
            return 0;
        }
        StageScoreInitialization in; in.total={100,200}; in.longTicks={10,20}; in.afterJustReflec={5,10};
        auto b=initializeStageScore(in);
        if(b.baseCount!=std::array<int32_t,2>{90,180} || b.lifeFloor!=100 || b.gauge[0]!=10000 || b.aggregates[0]!=425)
            throw Error("stage score original default mismatch");
        in.selectedChartSide=1; in.removeLifeFloor=true;
        b=initializeStageScore(in);
        if(b.baseCount[0]!=180 || b.lifeFloor!=0 || b.life[0]!=100) throw Error("stage score side mapping");
        std::cout<<"PASS 2 stage score initialization checks\n";
        return 0;
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
