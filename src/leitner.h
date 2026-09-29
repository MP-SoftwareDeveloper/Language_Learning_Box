#pragma once

// Pure Leitner scheduling rules: no Qt, no I/O, so it is trivially testable.
//
// Boxes 1..5 are active. A correct answer moves a card up one box and schedules
// it 2^(box-1) days out (1, 2, 4, 8, 16). A correct answer in box 5 retires the
// card to the "learned" box, where it is no longer reviewed. A wrong answer
// always sends the card back to box 1.

namespace leitner {

inline constexpr int kFirstBox = 1;
inline constexpr int kLastBox = 5;
inline constexpr int kLearnedBox = 6;

constexpr bool isActive(int box) { return box >= kFirstBox && box <= kLastBox; }

// Days until the next review for a card sitting in `box`; -1 = never (learned).
constexpr int intervalDays(int box)
{
    if (box < kFirstBox)
        return 0;
    if (box > kLastBox)
        return -1;
    return 1 << (box - 1);
}

struct Step
{
    int box;
    int intervalDays; // -1 = no further review
};

constexpr Step next(int box, bool correct)
{
    if (!correct)
        return {kFirstBox, intervalDays(kFirstBox)};
    const int nb = box < kFirstBox ? kFirstBox + 1 : (box >= kLastBox ? kLearnedBox : box + 1);
    return {nb, intervalDays(nb)};
}

// Manual move (box page): the card is scheduled as if it had just arrived in `box` by
// answering, i.e. the box's own interval (box 1 = tomorrow, Learned = never).
// Out-of-range boxes are clamped to 1..Learned.
constexpr Step moveTo(int box)
{
    const int b = box < kFirstBox ? kFirstBox : (box > kLearnedBox ? kLearnedBox : box);
    return {b, intervalDays(b)};
}

} // namespace leitner
