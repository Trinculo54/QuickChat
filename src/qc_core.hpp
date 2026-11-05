// QuickChat's rules, with no game or Windows in them, so the test can run them as they are
// (quickchat.cpp is the game side).
#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <string>

namespace qc {

// ---- the phrases ------------------------------------------------------------------------------
// The number is what crosses the network. Never renumber or reword one into a different meaning:
// a player on an older QuickChat would see the old phrase. New phrases go at the end.
constexpr const char* kPhrases[] = {
    "Nice race!",   // 0
    "Good game",    // 1
    "Thanks!",      // 2
    "Sorry!",       // 3
    "Follow me",    // 4
    "Wait up",      // 5
    "Race?",        // 6
    "Let's cruise", // 7
    "Nice car!",    // 8
    "Nice drift!",  // 9
    "Watch out!",   // 10
    "On my way",    // 11
    "Hello!",       // 12
    "Bye!",         // 13
    "Yes",          // 14
    "No",           // 15
};
constexpr uint32_t kPhraseCount = sizeof kPhrases / sizeof *kPhrases;

inline const char* phrase(uint32_t i) { return i < kPhraseCount ? kPhrases[i] : nullptr; }

// The four d-pad directions, in the order of the settings rows.
enum Dir { Up, Right, Down, Left, kDirs };
constexpr const char* kDirNames[kDirs] = {"Up", "Right", "Down", "Left"};
constexpr uint32_t kDefaultPhrase[kDirs] = {12, 2, 3, 0};   // Hello!, Thanks!, Sorry!, Nice race!

// ---- sending ----------------------------------------------------------------------------------

constexpr uint64_t kCooldownMs = 1500;   // between two messages from this player

struct Outbox {
    uint32_t seq = 0;            // messages sent this run; 0 = none
    uint32_t phrase = 0;
    uint64_t sent_ms = 0;

    // A press: true if it's a message (not too soon after the last one).
    bool send(uint32_t p, uint64_t now) {
        if (p >= kPhraseCount || (seq && now - sent_ms < kCooldownMs)) return false;
        ++seq;
        phrase  = p;
        sent_ms = now;
        return true;
    }
};

// ---- receiving --------------------------------------------------------------------------------

// Shows each (player, message number) once. The online mod repeats a message until it knows it
// arrived, so the same one comes many times; a player who restarts the game starts again at 1, so a
// number that differs from the last one seen counts as new, whether it's higher or not.
struct Inbox {
    std::map<uint32_t, uint32_t> last;   // player -> last message number seen

    bool fresh(uint32_t from, uint32_t seq) {
        auto it = last.find(from);
        if (it != last.end() && it->second == seq) return false;
        last[from] = seq;
        return true;
    }
};

// ---- pop-ups ----------------------------------------------------------------------------------

constexpr uint64_t kPopupGapMs = 3000;   // the game's notification needs about this long on screen
constexpr uint64_t kPopupMaxAgeMs = 9000;   // older than this when its turn comes: dropped
constexpr size_t kPopupQueue = 4;

struct Popup {
    std::string who, text;
    uint64_t at = 0;
};

struct PopupQueue {
    std::deque<Popup> q;
    uint64_t next_ms = 0;

    // A full queue drops its oldest: in a busy session the newest messages matter.
    void push(Popup p) {
        if (q.size() >= kPopupQueue) q.pop_front();
        q.push_back(std::move(p));
    }

    // The pop-up to show now, if any (stale ones are dropped on the way).
    bool take(uint64_t now, Popup& out) {
        if (now < next_ms) return false;
        while (!q.empty() && now - q.front().at > kPopupMaxAgeMs) q.pop_front();
        if (q.empty()) return false;
        out = std::move(q.front());
        q.pop_front();
        next_ms = now + kPopupGapMs;
        return true;
    }
};

// A gamertag fit for the pop-up: printable ASCII, at most 20 characters, "Someone" for none.
inline std::string clean_name(const char* name) {
    std::string s;
    if (name)
        for (const char* c = name; *c && s.size() < 20; ++c)
            if (*c >= 0x20 && *c < 0x7F) s += *c;
    return s.empty() ? "Someone" : s;
}

} // namespace qc
