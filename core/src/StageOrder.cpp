#include "rb/StageOrder.hpp"
#include <algorithm>
#include <utility>
namespace rb {
namespace {
int compare(const StageOrderEntry &a, const StageOrderEntry &b) {
    if (!a.active)
        return b.active ? 1 : 0;
    if (b.active)
        return a.relativeTick > b.relativeTick ? 1 : -1;
    return -1;
}
} // namespace
void sortStageOrder(std::vector<StageOrderEntry> &a) {
    if (a.size() < 2)
        return;
    std::vector<std::pair<int, int>> stack;
    int lo = 0, hi = int(a.size()) - 1;
    for (;;) {
        if (hi - lo + 1 <= 8) {
            for (int end = hi; end > lo; --end) {
                int maximum = lo;
                for (int i = lo + 1; i <= end; ++i)
                    if (compare(a[i], a[maximum]) > 0)
                        maximum = i;
                std::swap(a[maximum], a[end]);
            }
        } else {
            int mid = lo + (hi - lo + 1) / 2;
            if (compare(a[lo], a[mid]) > 0)
                std::swap(a[lo], a[mid]);
            if (compare(a[lo], a[hi]) > 0)
                std::swap(a[lo], a[hi]);
            if (compare(a[mid], a[hi]) > 0)
                std::swap(a[mid], a[hi]);
            int left = lo, right = hi;
            for (;;) {
                if (mid > left) {
                    do {
                        ++left;
                    } while (left < mid && compare(a[left], a[mid]) <= 0);
                }
                if (mid <= left) {
                    do {
                        ++left;
                    } while (left <= hi && compare(a[left], a[mid]) <= 0);
                }
                do {
                    --right;
                } while (right > mid && compare(a[right], a[mid]) > 0);
                if (left > right)
                    break;
                std::swap(a[left], a[right]);
                if (mid == right)
                    mid = left;
            }
            ++right;
            if (mid < right) {
                do {
                    --right;
                } while (right > mid && compare(a[right], a[mid]) == 0);
            }
            if (mid >= right) {
                do {
                    --right;
                } while (right > lo && compare(a[right], a[mid]) == 0);
            }
            if (right - lo >= hi - left) {
                if (lo < right)
                    stack.emplace_back(lo, right);
                if (left < hi) {
                    lo = left;
                    continue;
                }
            } else {
                if (left < hi)
                    stack.emplace_back(left, hi);
                if (lo < right) {
                    hi = right;
                    continue;
                }
            }
        }
        if (stack.empty())
            return;
        lo = stack.back().first;
        hi = stack.back().second;
        stack.pop_back();
    }
}
} // namespace rb
