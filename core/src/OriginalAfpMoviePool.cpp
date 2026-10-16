#include "rb/OriginalAfpMoviePool.hpp"

namespace rb {
namespace {
const std::array<OriginalAfpPoolDescriptor, 37> table{{
    {0, 12, 34, 30, 0, "judge_just", "judge_just", false},
    {1, 12, 34, 30, 0, "judge_great", "judge_great", false},
    {2, 12, 34, 30, 0, "judge_good", "judge_good", false},
    {3, 12, 34, 30, 0, "judge_keep", "judge_keep", false},
    {4, 12, 34, 30, 0, "judge_miss", "judge_miss", false},
    {5, 10, 101, 35, 1, "exp1_just", "exp1_just", false},
    {6, 10, 101, 35, 1, "exp1_bigbang", "exp1_bigbang", false},
    {7, 10, 36, 35, 1, "exp_keep", "exp_keep", false},
    {8, 10, 36, 70, 1, "exp_damage", "exp_damage", false},
    {9, 10, 101, 35, 1, "exp1_just", "exp1_just", false},
    {10, 10, 101, 35, 1, "exp1_bigbang", "exp1_bigbang", false},
    {11, 10, 36, 35, 1, "exp_keep", "exp_keep", false},
    {12, 10, 36, 70, 1, "exp_damage", "exp_damage", false},
    {13, 25, 34, 70, 1, "exp_kabe_red", "exp_kabe_blue", false},
    {14, 25, 34, 70, 1, "exp_kabe_red", "exp_kabe_blue", false},
    {15, 24, 33, 37, 0, "eff_long_tail_red", "eff_long_tail_blue", true},
    {16, 24, 33, 37, 0, "eff_long_tail_red", "eff_long_tail_blue", true},
    {17, 5, 33, 36, 0, "eff_justreflec", "eff_justreflec", true},
    {18, 5, 33, 36, 0, "eff_justreflec", "eff_justreflec", true},
    {19, 5, 33, 36, 0, "eff_justreflec_top", "eff_justreflec_top", true},
    {20, 5, 33, 36, 0, "eff_justreflec_top", "eff_justreflec_top", true},
    {21, 5, 33, 63, 0, "eff_just_head_red_01", "eff_just_head_blue_01", true},
    {22, 5, 33, 63, 0, "eff_just_head_red_01", "eff_just_head_blue_01", true},
    {23, 64, 33, 40, 0, "swich_on", "swich_on", true},
    {24, 64, 33, 40, 0, "swich_off", "swich_off", true},
    {25, 24, 33, 40, 0, "swich_on", "swich_on", true},
    {26, 24, 33, 40, 0, "swich_off", "swich_off", true},
    {27, 64, 33, 62, 0, "swich_long_body_on", "swich_long_body_on", true},
    {28, 64, 33, 62, 0, "swich_long_body_off", "swich_long_body_off", true},
    {29, 24, 33, 40, 0, "long_tail_bg_switch", "long_tail_bg_switch", true},
    {30, 24, 36, 61, 0, "exp_loop", "exp_loop", false},
    {31, 24, 33, 40, 0, "obj_bigbang_big_red", "obj_bigbang_big_blue", true},
    {32, 24, 33, 40, 0, "obj_bigbang_small_red", "obj_bigbang_small_blue", true},
    {33, 80, 33, 38, 0, "eff_justcollection", "eff_justcollection", true},
    {34, 48, 33, 38, 0, "eff_justcollection_top", "eff_justcollection_top", true},
    {35, 24, 33, 38, 0, "eff_justcollection_bigbang_s", "eff_justcollection_bigbang_s", true},
    {36, 24, 33, 38, 0, "eff_justcollection_bigbang_b", "eff_justcollection_bigbang_b", true},
}};
} // namespace
OriginalAfpMoviePool::OriginalAfpMoviePool() {
    for (const auto &d : table) {
        auto &p = pools_[d.poolIndex];
        p.slots.resize(d.capacity);
        p.head = d.capacity ? 0 : UINT32_MAX;
        for (uint32_t i = 0; i < d.capacity; ++i)
            p.slots[i].next = i + 1 < d.capacity ? i + 1 : UINT32_MAX;
    }
}
const std::array<OriginalAfpPoolDescriptor, 37> &OriginalAfpMoviePool::descriptors() {
    return table;
}
const OriginalAfpPoolDescriptor &OriginalAfpMoviePool::descriptor(uint32_t id) {
    if (id >= table.size())
        throw Error("AFP pool index outside original table");
    return table[id];
}
std::optional<uint32_t> OriginalAfpMoviePool::acquire(uint32_t id) {
    descriptor(id);
    auto &p = pools_[id];
    if (p.head == UINT32_MAX)
        return {};
    const auto result = p.head;
    auto &s = p.slots[result];
    p.head = s.next;
    s.next = UINT32_MAX;
    s.acquired = true;
    return result;
}
void OriginalAfpMoviePool::release(uint32_t id, uint32_t slot) {
    descriptor(id);
    auto &p = pools_[id];
    if (slot >= p.slots.size() || !p.slots[slot].acquired)
        throw Error("AFP pool invalid release");
    auto &s = p.slots[slot];
    s.state.visible = false;
    s.acquired = false;
    s.next = p.head;
    p.head = slot;
}
SwitchMovieState &OriginalAfpMoviePool::state(uint32_t id, uint32_t slot) {
    descriptor(id);
    auto &p = pools_[id];
    if (slot >= p.slots.size())
        throw Error("AFP pool slot outside capacity");
    return p.slots[slot].state;
}
} // namespace rb
