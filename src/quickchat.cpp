// QuickChat: pick a short phrase from ANNA's menu to send to the other players in an online session
// ("Thanks!", "Nice race!"). The messages show on the HUD, yours as "You", theirs with their gamertag.
//
// QuickChat sends nothing over the network itself: it publishes QuickChat.Bridge (include/quickchat.h)
// for the online mod to carry the messages, and only a phrase number ever crosses. Offline, or without
// that mod, your own messages still show on your screen.
//
// The phrases are a list of QuickChat's in FH2 ANNA's menu (its interface, include/fh2anna.h): D-pad down
// opens ANNA, Quick Chat is in its top list. ANNA has the d-pad while it's open, so nothing else reacts,
// and with ANNA's voice commands on a phrase can be said. Without FH2 ANNA there's no way to send, and
// other players' messages still show.
//
// Messages go to the platform's HUD feed (FH2Platform.HudLayer version 2, platform 2.9.0): a few lines at
// the left over the minimap, like a chat log. On a platform without it they fall back to pop-ups, with the
// same game calls as Achievements: the HUD's minor notification components are tracked through their
// constructor and destructor (game+0x73E0B1, game+0x73F046), and on the game thread's AchievementsModule
// tick (game+0x3FCD84) MakeDisplayString (game+0x323310) and ShowNotif (game+0x755330) show one, both
// byte-checked first.
//
// A message also shows as a box over its sender's car: the game's name tag page with a
// box added (src/pill_page.hpp), its text a page text of QuickChat's (platform 2.9.1, API 11), given to the tag
// that shows the sender's gamertag. See "the box over a car" below.
#include "fh2anna.h"
#include "fh2mod.h"
#include "fh2mod_hud.h"
#include "pill_page.hpp"
#include "qc_core.hpp"
#include "quickchat.h"
#include "unzip.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {

using namespace qc;

// ---- game addresses (forza_durango_release_final.exe of 24 Nov 2015 only) ----
constexpr uintptr_t RVA_NOTIF_CTOR = 0x73E0B1;   // HUDMinorNotificationComponent constructor, rbx = the component
constexpr uintptr_t RVA_NOTIF_DTOR = 0x73F046;   // its destructor, rbx = the component
constexpr uintptr_t RVA_TICK       = 0x3FCD84;   // AchievementsModule update: the game thread, about 60 times a second
constexpr uintptr_t RVA_MAKE_STR   = 0x323310;   // MakeDisplayString(out, const char*, int len)
constexpr uintptr_t RVA_SHOW_NOTIF = 0x755330;   // ShowNotif(component, main, sub, icon), takes the strings by value
const uint8_t kMakeStrBytes[]   = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x28, 0xFE, 0xFF, 0xFF, 0xFF};
const uint8_t kShowNotifBytes[] = {0x4C, 0x89, 0x4C, 0x24, 0x20, 0x4C, 0x89, 0x44, 0x24, 0x18, 0x48, 0x89, 0x54, 0x24, 0x10};

const FH2ModApi* g_api       = nullptr;
FH2Mod* g_mod                = nullptr;
const FH2HudInterface* g_hud = nullptr;   // the feed; null: pop-ups
uint8_t* g_base              = nullptr;
std::wstring g_ini;

// ---- settings: fh2-mods\QuickChat.ini ----------------------------------------------------------

std::atomic<bool> g_on{true}, g_others{true};
bool g_pill = true, g_pill_pop = true, g_pill_test = false;   // read once, at the start

void log(const std::string& s) {
    if (g_api && g_mod) g_api->log(g_mod, s.c_str());
}

int ini_int(const wchar_t* key, int def) { return static_cast<int>(GetPrivateProfileIntW(L"QuickChat", key, def, g_ini.c_str())); }
void ini_set(const wchar_t* key, int v) { WritePrivateProfileStringW(L"QuickChat", key, std::to_wstring(v).c_str(), g_ini.c_str()); }

void load_settings() {
    g_on     = ini_int(L"Enabled", 1) != 0;
    g_others = ini_int(L"ShowOthers", 1) != 0;
    g_pill      = ini_int(L"Pill", 1) != 0;
    g_pill_pop  = ini_int(L"PillPop", 1) != 0;
    g_pill_test = ini_int(L"PillTest", 0) != 0;
    // a file a player can read and edit, with what each key does
    if (GetFileAttributesW(g_ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
        const std::string t = "; QuickChat settings. Pause > MODS > YOUR MODS > QuickChat changes the first three too.\r\n"
                              "[QuickChat]\r\n"
                              "; 1 = on, 0 = off (off: the Quick Chat list leaves ANNA's menu)\r\n"
                              "Enabled=1\r\n"
                              "; 1 = show other players' messages, 0 = only send\r\n"
                              "ShowOthers=1\r\n"
                              "; 1 = a message also shows as a box over its sender's car, 0 = only in the chat log.\r\n"
                              "; Read when the game starts.\r\n"
                              "Pill=1\r\n"
                              "; 1 = the box pops up and fades away, 0 = it simply appears and goes (try 0 if the box\r\n"
                              "; never shows). Read when the game starts.\r\n"
                              "PillPop=1\r\n"
                              "; 1 = for trying the box offline: every 5 seconds a Drivatar near you says a phrase, one\r\n"
                              "; car after another, and the chat log names that Drivatar. Read when the game starts.\r\n"
                              "PillTest=0\r\n";
        HANDLE h = CreateFileW(g_ini.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD put = 0;
            WriteFile(h, t.data(), static_cast<DWORD>(t.size()), &put, nullptr);
            CloseHandle(h);
        }
    }
}

// ---- messages ---------------------------------------------------------------------------------

std::mutex g_mu;   // the outbox, inbox and pop-ups
Outbox g_out;
Inbox g_in;
PopupQueue g_pop;
std::atomic<bool> g_told_offline{false};

// A message on screen: a feed line, or a pop-up when there's no feed.
void show_message(const std::string& who, const std::string& text) {
    if (g_hud) {
        FH2HudFeedLine l{};
        l.size  = sizeof l;
        l.name  = who.c_str();
        l.text  = text.c_str();
        l.style = FH2_HUD_FEED_GREEN;
        if (g_hud->post_hud_feed(g_mod, &l)) return;
    }
    std::lock_guard<std::mutex> lk(g_mu);
    g_pop.push({who, text, GetTickCount64()});
}

// ---- ANNA's menu: a Quick Chat list in the top list, one entry a phrase ---------------------------
// The entry's `user` is the phrase's number. on_choose runs on the game's thread once ANNA has shown the
// choice and closed.

int anna_shown(void*, const char*) { return g_on ? 1 : 0; }   // off: the entries hide, and the list with them

void pill_show(const std::string& name, uint32_t phrase);   // the box over a car, below

void anna_choose(void* user, const char*) {
    const uint32_t p = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(user));
    if (!g_on || p >= kPhraseCount) return;
    bool sent;
    uint32_t seq;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        sent = g_out.send(p, GetTickCount64());
        seq  = g_out.seq;
    }
    if (!sent) return;   // too soon after the last one
    show_message("You", kPhrases[p]);
    log("sent " + std::to_string(seq) + ": " + kPhrases[p]);
    if (!(g_api->api_version >= 9 && g_api->playing_online && g_api->playing_online()) && !g_told_offline.exchange(true))
        log("not playing online: your messages only show on your own screen");
}

// From the worker, once every mod has started (ANNA publishes its interface in its own start).
void add_to_anna() {
    g_api->wait_for_mods(30000);
    auto* anna = static_cast<const FH2AnnaApi*>(g_api->get_interface(FH2ANNA_INTERFACE, 2));
    if (!anna || anna->size < offsetof(FH2AnnaApi, add_entry) + sizeof anna->add_entry || !anna->add_entry) {
        log("FH2 ANNA isn't installed (or is older than its menu for other mods): there's no way to send. Other players' "
            "messages still show");
        return;
    }
    FH2AnnaEntry list{};
    list.size  = sizeof list;
    list.key   = "quickchat";
    list.label = "Quick Chat";
    bool ok    = anna->add_entry(&list) != 0;
    static std::string keys[kPhraseCount];
    for (uint32_t p : kMenuOrder) {
        keys[p] = "quickchat." + std::to_string(p);
        FH2AnnaEntry e{};
        e.size      = sizeof e;
        e.key       = keys[p].c_str();
        e.parent    = "quickchat";
        e.label     = kPhrases[p];
        e.on_choose = &anna_choose;
        e.shown     = &anna_shown;
        e.user      = reinterpret_cast<void*>(static_cast<uintptr_t>(p));
        ok          = anna->add_entry(&e) != 0 && ok;
    }
    log(ok ? "Quick Chat is in ANNA's menu: D-pad down, Quick Chat, then a phrase"
           : "ANNA didn't take every Quick Chat entry: some phrases can't be sent");
}

// ---- the bridge -------------------------------------------------------------------------------

int bridge_latest(uint32_t* seq, uint32_t* phrase, uint64_t* sent_ms) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_out.seq) return 0;
    if (seq) *seq = g_out.seq;
    if (phrase) *phrase = g_out.phrase;
    if (sent_ms) *sent_ms = g_out.sent_ms;
    return 1;
}

int bridge_deliver(uint32_t from, const char* name, uint32_t seq, uint32_t p) {
    if (p >= kPhraseCount) return 0;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (!g_in.fresh(from, seq)) return 0;
    }
    const std::string who = clean_name(name);
    log("from " + who + ": " + kPhrases[p]);
    if (!g_on || !g_others) return 0;
    show_message(who, kPhrases[p]);
    pill_show(who, p);
    return 1;
}

const char* bridge_text(uint32_t p) { return phrase(p); }

const QuickChatBridge g_bridge = {sizeof(QuickChatBridge), QUICKCHAT_BRIDGE_VERSION, kPhraseCount, bridge_latest, bridge_deliver,
                                  bridge_text};

// ---- the HUD notification (game thread) --------------------------------------------------------

bool sread(const void* p, void* out, size_t n) {
    if (reinterpret_cast<uintptr_t>(p) < 0x10000) return false;
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), p, out, n, &got) && got == n;
}
template <class T> T srd(const void* p, T def = T{}) {
    T v;
    return sread(p, &v, sizeof v) ? v : def;
}
// MSVC 2012 std::string: buffer or pointer at +0, size at +0x10, capacity at +0x18.
std::string msvc_str(const void* s) {
    auto b              = static_cast<const uint8_t*>(s);
    const uint64_t size = srd<uint64_t>(b + 0x10, ~0ull), cap = srd<uint64_t>(b + 0x18, 0);
    if (size > 512 || cap < size) return {};
    const char* p = cap >= 16 ? srd<const char*>(b) : reinterpret_cast<const char*>(b);
    std::string r(size, '\0');
    if (size && !sread(p, r.data(), size)) return {};
    return r;
}
bool code_matches(uintptr_t rva, const uint8_t* bytes, size_t n) {
    uint8_t have[32];
    return n <= sizeof have && sread(g_base + rva, have, n) && std::memcmp(have, bytes, n) == 0;
}

std::mutex g_comp_mu;
std::vector<uint8_t*> g_comps;   // live HUDMinorNotificationComponents, oldest first
std::atomic<bool> g_popup_hooks{false};

void on_notif_ctor(FH2Context* c, void*) {
    if (!g_popup_hooks) return;
    std::lock_guard<std::mutex> lk(g_comp_mu);
    g_comps.push_back(reinterpret_cast<uint8_t*>(c->rbx));
}
void on_notif_dtor(FH2Context* c, void*) {
    std::lock_guard<std::mutex> lk(g_comp_mu);
    auto it = std::find(g_comps.begin(), g_comps.end(), reinterpret_cast<uint8_t*>(c->rbx));
    if (it != g_comps.end()) g_comps.erase(it);
}

// The HUD's "road discovered" notification (an icon and two lines; it lives as long as the HUD), else
// the oldest one.
uint8_t* pick_component() {
    std::lock_guard<std::mutex> lk(g_comp_mu);
    for (uint8_t* c : g_comps)
        if (msvc_str(c + 0x128 + 0x18) == "RoadDiscovery") return c;
    return g_comps.empty() ? nullptr : g_comps.front();
}

// No objects with destructors here (__try).
bool show_notif(uint8_t* comp, const char* who, const char* text) {
    auto make = reinterpret_cast<void** (*)(void**, const char*, int)>(g_base + RVA_MAKE_STR);
    auto show = reinterpret_cast<void (*)(void*, void**, void**, void**)>(g_base + RVA_SHOW_NOTIF);
    __try {
        void *a = nullptr, *b = nullptr, *icon = nullptr;
        make(&a, who, -1);
        make(&b, text, -1);
        make(&icon, "CarMeet", -1);   // the HUD's car meet icon (Controls/HUDMinorNotificationControl.xaml)
        show(comp, &a, &b, &icon);    // releases all three itself
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool g_calls_checked = false, g_calls_ok = false;   // game thread only

void on_tick(FH2Context*, void*) {
    if (!g_popup_hooks) return;
    Popup p;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_pop.q.empty() || !g_pop.take(GetTickCount64(), p)) return;
    }
    if (!g_calls_checked) {
        g_calls_checked = true;
        g_calls_ok      = code_matches(RVA_MAKE_STR, kMakeStrBytes, sizeof kMakeStrBytes) &&
                     code_matches(RVA_SHOW_NOTIF, kShowNotifBytes, sizeof kShowNotifBytes);
        if (!g_calls_ok) log("pop-ups are off: the game's notification code doesn't match");
    }
    if (!g_calls_ok) return;
    uint8_t* comp = pick_component();
    if (!comp) {
        log("pop-up dropped (no HUD notification yet): " + p.who + ": " + p.text);
        return;
    }
    if (!show_notif(comp, p.who.c_str(), p.text.c_str())) log("pop-up failed (the game's code faulted): " + p.who + ": " + p.text);
}

FH2Hook* hook(const char* name, uintptr_t at, FH2InstructionFn fn) {
    FH2Hook* h = g_api->hook_instruction(g_mod, name, g_base + at, fn, nullptr);
    if (!h) log(std::string("couldn't hook ") + name + " (platform.log says why)");
    return h;
}

// ---- the box over a car ------------------------------------------------------------------------
// The game has 12 name tags, each a page object of its OpponentLabel class (vtable game+0x2E11A68), made when the
// game starts and handed to the cars near you. Each asks QuickChat for its box's text (the page texts), and
// QuickChat answers by the gamertag the tag is showing:
//   - OpponentLabel's UserProfile property (registered at game+0xDF01E0 with the getter game+0xDBC9D0, which
//     calls game+0xCBA990) is the object at the tag's +0x410;
//   - a profile's FormattedGamertag (the getter game+0xC2A5C0, which calls game+0xBDF6A0) is the string at the
//     profile's +0x60: UTF-16 text at its +0x28, the length at +0x30.
// All four pieces of code are byte-checked once; if one differs there are no boxes.
constexpr uintptr_t RVA_LABEL_VTABLE = 0x2E11A68;
struct CodeCheck {
    uintptr_t rva;
    const uint8_t* bytes;
    size_t n;
};
const uint8_t kLabelProfileReg[] = {0x48, 0x8D, 0x05, 0xA9, 0xC7, 0xFC, 0xFF};   // lea rax, [game+0xDBC9D0]
const uint8_t kLabelProfileGet[] = {0x48, 0x8D, 0x54, 0x24, 0x28, 0x49, 0x8B, 0xC8, 0xE8, 0x8D, 0xDF, 0xEF, 0xFF};   // call game+0xCBA990
const uint8_t kLabelProfileAt[]  = {0x48, 0x83, 0xEC, 0x18, 0x48, 0x8B, 0x81, 0x10, 0x04, 0x00, 0x00};   // mov rax, [rcx + 0x410]
const uint8_t kProfileTagGet[]   = {0x48, 0x8D, 0x54, 0x24, 0x28, 0x49, 0x8B, 0xC8, 0xE8, 0xAD, 0x50, 0xFB, 0xFF};   // call game+0xBDF6A0
const uint8_t kProfileTagAt[]    = {0x48, 0x83, 0xEC, 0x18, 0x48, 0x8B, 0x41, 0x60};   // mov rax, [rcx + 0x60]
const CodeCheck kTagCode[]       = {{0xDF0220, kLabelProfileReg, sizeof kLabelProfileReg},
                                    {0xDBC9F6, kLabelProfileGet, sizeof kLabelProfileGet},
                                    {0xCBA990, kLabelProfileAt, sizeof kLabelProfileAt},
                                    {0xC2A5E6, kProfileTagGet, sizeof kProfileTagGet},
                                    {0xBDF6A0, kProfileTagAt, sizeof kProfileTagAt}};

bool g_pill_page = false;            // the name tag page has the box (set in fh2mod_init)
std::atomic<int> g_tag_code{0};      // the code above: 0 not checked yet, 1 matches, -1 doesn't
std::mutex g_pill_mu;                // the boxes and the tags
Pills g_pills;
std::vector<void*> g_tags;           // the name tags that have asked for a text

// The gamertag a name tag is showing, ASCII only (as clean_name); empty for none, or a page that isn't a name tag.
std::string tag_name(const void* page) {
    auto* base = static_cast<uint8_t*>(g_api->game_base());
    if (!base) return {};
    if (!g_tag_code) {
        bool ok = true;
        uint8_t have[16];
        for (const CodeCheck& c : kTagCode) ok = ok && sread(base + c.rva, have, c.n) && std::memcmp(have, c.bytes, c.n) == 0;
        if (g_tag_code.exchange(ok ? 1 : -1) == 0 && !ok) log("the box over cars is off: the game's name tag code doesn't match");
    }
    if (g_tag_code != 1 || srd<uint8_t*>(page) != base + RVA_LABEL_VTABLE) return {};
    auto* profile = srd<const uint8_t*>(static_cast<const uint8_t*>(page) + 0x410);
    auto* str     = profile ? srd<const uint8_t*>(profile + 0x60) : nullptr;
    if (!str) return {};
    const int32_t len = srd<int32_t>(str + 0x30, 0);
    wchar_t w[64];
    if (len <= 0 || len > 64 || !sread(srd<const wchar_t*>(str + 0x28), w, len * sizeof(wchar_t))) return {};
    std::string s;
    for (int i = 0; i < len; ++i)
        if (w[i] >= 0x20 && w[i] < 0x7F) s += static_cast<char>(w[i]);
    return s;
}

// A name tag asks for its box's text (the phrase), or with `user` set for its "on" text. On the game's UI thread.
void pill_text(void* user, void* page, char* out, size_t size) {
    if (!size) return;
    out[0] = 0;
    {
        std::lock_guard<std::mutex> lk(g_pill_mu);
        if (g_tags.size() < 64 && std::find(g_tags.begin(), g_tags.end(), page) == g_tags.end()) g_tags.push_back(page);
        if (g_pills.by_name.empty()) return;   // no box anywhere: the game's memory isn't read
    }
    if (!g_on) return;
    const std::string name = tag_name(page);
    std::lock_guard<std::mutex> lk(g_pill_mu);
    if (const Pills::Pill* p = g_pills.find(name, GetTickCount64()))
        std::snprintf(out, size, "%s", user ? (p->n & 1 ? "1" : "2") : kPhrases[p->phrase]);
}

// Has every name tag ask again (on the game's thread, at its next tick).
void pill_refresh() {
    g_api->page_text_changed(g_mod, kPillText, nullptr);
    g_api->page_text_changed(g_mod, kPillOn, nullptr);
}

void pill_show(const std::string& name, uint32_t p) {
    if (!g_pill_page || !g_on || p >= kPhraseCount) return;
    {
        std::lock_guard<std::mutex> lk(g_pill_mu);
        g_pills.show(name, p, GetTickCount64());
    }
    pill_refresh();
}

// PillTest=1, for trying the box offline, where nobody sends you anything: a Drivatar "says" the next phrase, one
// name tag after another, as if the online mod had delivered it. The reads here aren't on the game's thread; they're
// guarded, and a tag caught mid-change only gives a wrong name once.
void pill_test_step(uint32_t step) {
    static bool told_none = false;
    std::vector<void*> tags;
    {
        std::lock_guard<std::mutex> lk(g_pill_mu);
        tags = g_tags;
    }
    std::vector<std::string> names;
    for (void* t : tags) {
        const std::string n = tag_name(t);
        if (!n.empty() && std::find(names.begin(), names.end(), n) == names.end()) names.push_back(n);
    }
    if (names.empty()) {
        if (!told_none) log("PillTest: none of the " + std::to_string(tags.size()) + " name tags has a gamertag yet (drive near other cars)");
        told_none = true;
        return;
    }
    told_none              = false;
    const std::string& who = names[step % names.size()];
    const uint32_t p       = kMenuOrder[step % kPhraseCount];
    log("PillTest: " + who + " says " + kPhrases[p] + " (" + std::to_string(names.size()) + " of " + std::to_string(tags.size()) +
        " name tags have a gamertag)");
    show_message(who, kPhrases[p]);
    pill_show(who, p);
}

std::string read_file(const std::wstring& path) {
    std::string d;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return d;
    LARGE_INTEGER size{};
    if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < (64 << 20)) {
        d.resize(static_cast<size_t>(size.QuadPart));
        DWORD got = 0;
        if (!ReadFile(h, d.data(), static_cast<DWORD>(d.size()), &got, nullptr) || got != d.size()) d.clear();
    }
    CloseHandle(h);
    return d;
}

// From fh2mod_init: the game reads the name tag page when it makes its name tags, as it starts.
void add_pill(const FH2ModApi* api) {
    if (!g_on || !g_pill) {
        log("the box over cars is off (Pill and Enabled in QuickChat.ini, read when the game starts)");
        return;
    }
    if (api->api_version < 11 || api->size < offsetof(FH2ModApi, page_text_changed) + sizeof api->page_text_changed) {
        log("this platform can't put a mod's text on the game's name tags (2.9.1 can): messages show in the chat log, "
            "without the box over the sender's car");
        return;
    }
    // FH2MP owns the name tag page (its developer badge), and in an FH2MP newer than 1.5.1 that page has this box too, reading the same
    // texts: only one mod can replace a game file, and FH2MP loads first. So with FH2MP here only the texts are added.
    if (GetModuleHandleW(L"FH2MP.dll")) {
        if (!api->add_page_text(g_mod, kPillText, &pill_text, nullptr) || !api->add_page_text(g_mod, kPillOn, &pill_text, &g_pill_page)) {
            log("no box over cars: the platform refused its texts (the line above says why)");
            return;
        }
        g_pill_page = true;
        log("FH2MP has the name tag page, so a message shows as a box over its sender's car in FH2MP's page (an FH2MP newer than 1.5.1; "
            "newer; an older FH2MP has no box, and messages only show in the chat log). FH2MP shows no box on a Drivatar's tag" +
            std::string(g_pill_test ? ", so PillTest's offline phrases don't show" : ""));
        return;
    }
    const std::wstring zip = std::wstring(api->game_dir ? api->game_dir : L"") + L"\\media\\UI.zip";
    std::string why;
    const std::string page = with_pill(unzip_from(read_file(zip), kPillPagePath), g_pill_pop, &why);
    if (page.empty()) {
        log("no box over cars: the game's name tag page " + (why.empty() ? std::string("couldn't be read from media\\UI.zip") : "isn't as expected (" + why + ")"));
        return;
    }
    if (!api->add_page_text(g_mod, kPillText, &pill_text, nullptr) || !api->add_page_text(g_mod, kPillOn, &pill_text, &g_pill_page) ||
        !api->set_file(g_mod, (std::string("UI/") + kPillPagePath).c_str(), page.data(), page.size())) {
        log("no box over cars: the platform refused its texts or the name tag page (the line above says why)");
        return;
    }
    g_pill_page = true;
    log(std::string("a message also shows as a box over its sender's car") + (g_pill_pop ? "" : " (PillPop=0: it appears and goes without the pop)") +
        (g_pill_test ? "; PillTest=1: every 5 seconds a Drivatar says a phrase, one name tag after another" : ""));
}

DWORD WINAPI pill_worker(LPVOID) {   // takes a box's text away again once it has run out
    for (uint32_t tick = 1;; ++tick) {
        Sleep(200);
        bool gone;
        {
            std::lock_guard<std::mutex> lk(g_pill_mu);
            gone = g_pills.expire(GetTickCount64());
        }
        if (gone) pill_refresh();
        if (g_pill_test && g_on && tick % 25 == 0) pill_test_step(tick / 25);   // every 5 seconds
    }
}

DWORD WINAPI worker(LPVOID) {
    add_to_anna();
    g_base = static_cast<uint8_t*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!g_base) {
        log("the game's code never became ready: no pop-ups");
        return 0;
    }
    // the pop-ups are the feed's fallback too: if the HUD layer is off this session, a post fails and the
    // message pops up instead
    FH2Hook* ctor = hook("HUDMinorNotificationComponent constructor (QuickChat)", RVA_NOTIF_CTOR, &on_notif_ctor);
    FH2Hook* dtor = hook("HUDMinorNotificationComponent destructor (QuickChat)", RVA_NOTIF_DTOR, &on_notif_dtor);
    FH2Hook* tick = hook("AchievementsModule update (QuickChat pop-ups)", RVA_TICK, &on_tick);
    bool all      = ctor && dtor && tick;
    // the pop-ups need all three: without the destructor the list would keep components the game has freed
    g_popup_hooks = all;
    if (all && !g_api->enable_hooks(g_mod, nullptr))
        all = g_api->enable_hooks(g_mod, ctor) && g_api->enable_hooks(g_mod, dtor) && g_api->enable_hooks(g_mod, tick);
    if (!all) {
        g_popup_hooks = false;
        for (FH2Hook* h : {ctor, dtor, tick})
            if (h) g_api->disable_hooks(g_mod, h);
        log("the HUD notification hooks aren't all in: no pop-ups (messages are still sent)");
    } else {
        log(g_hud ? "messages go to the HUD feed, over the minimap; pop-ups if it's off" : "pop-ups are on");
    }
    return 0;
}

// ---- the Mods page ----------------------------------------------------------------------------

void on_setting(void*, const char* key, int v) {
    const std::string k = key;
    if (k == "Enabled") {
        g_on = v != 0;
        ini_set(L"Enabled", v != 0);
    } else if (k == "ShowOthers") {
        g_others = v != 0;
        ini_set(L"ShowOthers", v != 0);
    } else if (k == "Pill") {
        ini_set(L"Pill", v != 0);   // read at the next start
    }
}

void add_rows(const FH2ModApi* api) {
    FH2Setting on{};
    on.size        = sizeof on;
    on.key         = "Enabled";
    on.label       = "Quick chat";
    on.description = "A Quick Chat list in ANNA's menu (D-pad down while driving): pick a phrase to send it to the other "
                     "players. Off: the list leaves the menu, and other players' messages don't show.";
    on.value       = g_on ? 1 : 0;
    on.on_change   = &on_setting;
    api->add_setting(g_mod, &on);

    FH2Setting others = on;
    others.key         = "ShowOthers";
    others.label       = "Quick chat: other players' messages";
    others.description = "Off: you still send, but other players' messages don't pop up.";
    others.value       = g_others ? 1 : 0;
    api->add_setting(g_mod, &others);

    FH2Setting box = on;
    box.key           = "Pill";
    box.label         = "Quick chat: box over cars";
    box.description   = "A message also shows as a box over its sender's car, as well as in the chat log. Needs platform "
                         "2.9.1 or newer. A change takes effect the next time the game starts.";
    box.value         = g_pill ? 1 : 0;
    box.needs_restart = 1;
    api->add_setting(g_mod, &box);
}

} // namespace

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    g_api = api;
    FH2ModInfo info{};
    info.size             = sizeof info;
    info.name             = "QuickChat";
    info.version          = QC_VERSION;
    info.platform_version = "2.6.0";   // what it needs: instruction hooks. The HUD feed (2.9.0) is used when it's there
    info.github_repo      = "Trinculo54/QuickChat";
    info.check_updates    = 1;   // the MODS tab says when a newer release is out
    if (api->api_version >= 6) info.description = "Quick phrases for the other players online, from a Quick Chat list in ANNA's menu.";
    g_mod = api->register_mod(&info);
    if (!g_mod) return 0;
    if (api->api_version < 7) {   // instruction hooks (the pop-ups) came with platform 2.6.0
        log("this platform is older than 2.6.0: quick chat is off");
        return 0;
    }

    g_ini = std::wstring(api->azimuth_dir && *api->azimuth_dir ? api->azimuth_dir : L".") + L"\\fh2-mods\\QuickChat.ini";
    load_settings();
    add_rows(api);
    api->publish_interface(g_mod, QUICKCHAT_BRIDGE, QUICKCHAT_BRIDGE_VERSION, &g_bridge);

    // the HUD feed (platform 2.9.0): a layer with only the feed, which never takes the screen
    g_hud = static_cast<const FH2HudInterface*>(api->get_interface(FH2_HUD_INTERFACE, 2));
    if (g_hud && (g_hud->size < sizeof(FH2HudInterface) || g_hud->version < 2)) g_hud = nullptr;
    if (g_hud) {
        FH2HudLayer layer{};
        layer.size  = sizeof layer;
        layer.parts = FH2_HUD_PART_FEED;
        if (!g_hud->add_hud_layer(g_mod, &layer)) {
            g_hud = nullptr;
            log("the HUD feed was refused (the line above says why): messages show as pop-ups");
        }
    } else {
        log("this platform has no HUD feed: messages show as pop-ups");
    }

    add_pill(api);
    if (g_pill_page)
        if (HANDLE t = CreateThread(nullptr, 0, pill_worker, nullptr, 0, nullptr)) CloseHandle(t);

    if (HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr)) CloseHandle(t);
    log(g_on ? "on" : "off (Quick chat, on its page under Pause > MODS)");
    return 0;
}
