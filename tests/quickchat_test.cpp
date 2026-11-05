// QuickChat's rules (src/qc_core.hpp), with no game. Exits 0 when every check passes.
#include "qc_core.hpp"
#include "quickchat.h"

#include <cstdio>
#include <cstring>

using namespace qc;

namespace {
int g_failed = 0;
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failed;
}
} // namespace

int main() {
    // the phrases: numbers are what crosses the network
    check(kPhraseCount == 16, "16 phrases");
    check(std::strcmp(phrase(0), "Nice race!") == 0 && std::strcmp(phrase(15), "No") == 0, "phrase numbers keep their meaning");
    check(phrase(kPhraseCount) == nullptr, "no phrase past the end");
    bool short_enough = true;
    for (uint32_t i = 0; i < kPhraseCount; ++i) short_enough = short_enough && std::strlen(kPhrases[i]) <= 16;
    check(short_enough, "every phrase fits the notification's line (16 characters at most)");
    for (int d = 0; d < kDirs; ++d) check(kDefaultPhrase[d] < kPhraseCount, "default phrases exist");

    // sending: one message per press, not faster than the cooldown
    {
        Outbox o;
        check(o.send(2, 1000) && o.seq == 1 && o.phrase == 2, "the first press sends message 1");
        check(!o.send(3, 1000 + kCooldownMs - 1) && o.seq == 1 && o.phrase == 2, "a press inside the cooldown is ignored");
        check(o.send(3, 1000 + kCooldownMs) && o.seq == 2 && o.phrase == 3, "after the cooldown: message 2");
        check(!o.send(kPhraseCount, 99999) && o.seq == 2, "a phrase that doesn't exist isn't sent");
    }

    // receiving: each (player, message) once; a restart starts again at 1
    {
        Inbox in;
        check(in.fresh(7, 1), "player 7's message 1: shown");
        check(!in.fresh(7, 1), "the same again (the online mod repeats it): not shown");
        check(in.fresh(7, 2) && in.fresh(9, 2), "message 2, and another player's message 2: shown");
        check(in.fresh(7, 1), "player 7 restarted (message 1 again after 2): shown");
    }

    // pop-ups: spaced, the oldest dropped when full, stale ones dropped
    {
        PopupQueue q;
        Popup p;
        for (int i = 0; i < 6; ++i) q.push({"P" + std::to_string(i), "x", 1000});
        check(q.q.size() == kPopupQueue && q.q.front().who == "P2", "a full queue keeps the newest four");
        check(q.take(1000, p) && p.who == "P2", "the first shows straight away");
        check(!q.take(1000 + kPopupGapMs - 1, p), "the next waits for the gap");
        check(q.take(1000 + kPopupGapMs, p) && p.who == "P3", "then the next one");
        check(!q.take(1000 + kPopupMaxAgeMs + kPopupGapMs * 2, p) && q.q.empty(), "too old by its turn: dropped");
    }

    check(clean_name(nullptr) == "Someone" && clean_name("") == "Someone", "no gamertag: Someone");
    check(clean_name("Trinculo54") == "Trinculo54", "a gamertag as it is");
    check(clean_name("a\x01" "b\xC3\xA9" "c") == "abc", "control and non-ASCII bytes are dropped");
    check(clean_name("ABCDEFGHIJKLMNOPQRSTUVWXYZ").size() == 20, "at most 20 characters");

    std::printf("%s\n", g_failed ? "SOME CHECKS FAILED" : "all checks passed");
    return g_failed ? 1 : 0;
}
