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

// The order the phrases have in ANNA's Quick Chat list: the ones said most first, so they're a press or two away.
// Every phrase is in it once; a new phrase needs a place here too.
constexpr uint32_t kMenuOrder[] = {
    12, 2, 3, 0,   // Hello!, Thanks!, Sorry!, Nice race!
    1, 8, 9,       // Good game, Nice car!, Nice drift!
    4, 5, 11,      // Follow me, Wait up, On my way
    6, 7, 10,      // Race?, Let's cruise, Watch out!
    14, 15, 13,    // Yes, No, Bye!
};
static_assert(sizeof kMenuOrder / sizeof *kMenuOrder == kPhraseCount, "every phrase has a place in the list");

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

// ---- the box over a car ----------------------------------------------------------------------
// A message also shows as a box over its sender's car, on the name tag that carries their gamertag. The tag is
// found by that gamertag: it's what the online mod gives with a message, and what the tag itself shows.

// The box's animation takes 2.42 s (pill_page.hpp), but it starts when the tag reads the text, which for a car coming into view is
// after the message came: the text stays long enough that such a box still floats and fades with it, not empty.
constexpr uint64_t kPillMs = 6000;

// A gamertag as the boxes are keyed: the pop-up's cleaning, in lower case. Empty for no gamertag (never matched).
inline std::string name_key(const std::string& name) {
    std::string s;
    for (char c : name)
        if (s.size() < 20 && c >= 0x20 && c < 0x7F) s += static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
    return s;
}

struct Pills {
    struct Pill {
        uint32_t phrase = 0;
        uint64_t until  = 0;
        uint32_t n      = 0;   // 1 for a box on a tag that had none, one more for each that takes its place: odd and
                               // even take turns as the tag's "on" text, so the new one pops up again
    };
    std::map<std::string, Pill> by_name;   // name_key -> the box on that player's tag

    void show(const std::string& name, uint32_t phrase, uint64_t now) {
        const std::string k = name_key(name);
        if (k.empty()) return;
        auto it          = by_name.find(k);
        const uint32_t n = it == by_name.end() ? 1 : it->second.n + 1;
        by_name[k]       = {phrase, now + kPillMs, n};
    }
    // The box for the tag that shows `name`, if it has one that hasn't run out.
    const Pill* find(const std::string& name, uint64_t now) const {
        auto it = by_name.find(name_key(name));
        return it != by_name.end() && now < it->second.until ? &it->second : nullptr;
    }
    // Drops the boxes that have run out: true if any did (the tags then need telling).
    bool expire(uint64_t now) {
        return std::erase_if(by_name, [now](const auto& kv) { return now >= kv.second.until; }) != 0;
    }
};

} // namespace qc
