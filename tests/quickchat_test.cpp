// QuickChat's rules (src/qc_core.hpp), with no game. Exits 0 when every check passes.
#include "pill_page.hpp"
#include "qc_core.hpp"
#include "quickchat.h"
#include "unzip.hpp"

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

// quickchat_test.exe [<the game's media\UI.zip> <a name to write the two pages under>]
int main(int argc, char** argv) {
    // the phrases: numbers are what crosses the network
    check(kPhraseCount == 16, "16 phrases");
    check(std::strcmp(phrase(0), "Nice race!") == 0 && std::strcmp(phrase(15), "No") == 0, "phrase numbers keep their meaning");
    check(phrase(kPhraseCount) == nullptr, "no phrase past the end");
    bool short_enough = true;
    for (uint32_t i = 0; i < kPhraseCount; ++i) short_enough = short_enough && std::strlen(kPhrases[i]) <= 16;
    check(short_enough, "every phrase fits the notification's line (16 characters at most)");
    // ANNA's list: every phrase once, Hello! first
    {
        unsigned seen = 0;
        bool once     = true;
        for (uint32_t p : kMenuOrder) {
            once = once && p < kPhraseCount && !(seen & (1u << p));
            if (p < kPhraseCount) seen |= 1u << p;
        }
        check(once && seen == (1u << kPhraseCount) - 1, "the list has every phrase exactly once");
        check(kMenuOrder[0] == 12, "Hello! is first in the list");
    }

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

    // the box over a car: found by gamertag, gone when it has run out, a new message takes its place
    {
        Pills ps;
        ps.show("AcidCatFish", 2, 1000);
        const Pills::Pill* p = ps.find("acidcatfish", 1000);
        check(p && p->phrase == 2 && p->n == 1, "a box is found by its gamertag, whatever the case");
        check(!ps.find("Someone else", 1000) && !ps.find("", 1000), "another tag, or one with no gamertag, has none");
        ps.show("AcidCatFish", 3, 1500);
        p = ps.find("AcidCatFish", 1500);
        check(p && p->phrase == 3 && p->n == 2, "a new message takes the box's place, with the other \"on\" text");
        check(!ps.expire(1500 + kPillMs - 1) && ps.find("AcidCatFish", 1500 + kPillMs - 1), "it lasts kPillMs");
        check(!ps.find("AcidCatFish", 1500 + kPillMs) && ps.expire(1500 + kPillMs) && ps.by_name.empty(), "then it's gone");
        ps.show("", 1, 1000);
        check(ps.by_name.empty(), "no gamertag: no box");
    }

    // the name tag page: the box goes in once, after the row of icons, and only into the page expected
    {
        const std::string page = "<Page><Page.Resources><local:StringVisConverter x:Key=\"A_Converter_StringToVis\" /></Page.Resources>"
                                 "<Grid><StackPanel><local:FZMaterial x:Name=\"HVTIcon\" /></StackPanel></Grid></Page>";
        std::string why;
        const std::string pop = with_pill(page, true, &why), plain = with_pill(page, false, &why);
        auto count = [](const std::string& s, const char* what) {
            size_t n = 0;
            for (size_t at = s.find(what); at != std::string::npos; at = s.find(what, at + 1)) ++n;
            return n;
        };
        check(count(pop, "{Binding FH2Mod_Text_QuickChat_Pill}") == 1 && count(pop, "{Binding FH2Mod_Text_QuickChat_PillOn}") == 2,
              "the popping box: one text, and a trigger for each \"on\" text");
        check(pop.find("QuickChat_ControlTemplate_Pill\"") < pop.find("</Page.Resources>") &&
                  pop.find("<Control Template=") > pop.find("</StackPanel>") && pop.find("<Control Template=") < pop.find("</Grid></Page>"),
              "its template is in the resources, its place after the row of icons");
        check(count(plain, "FH2Mod_Text_QuickChat_Pill") == 2 && count(plain, "PillOn") == 0 && count(plain, "Storyboard") == 0,
              "the plain box: shown while it has a text, nothing animated");
        check(with_pill(pop, true, &why).empty() && why == "it already has the box", "a page that has the box isn't given another");
        check(with_pill("<Page></Page>", true, &why).empty() && !why.empty(), "a page that isn't the name tag page is refused, with why");
    }

    // with the game's UI.zip: its real name tag page, with the box, for a look (written beside the zip's name given)
    if (argc > 2) {
        std::string zip;
        if (FILE* f = std::fopen(argv[1], "rb")) {
            char buf[65536];
            for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) zip.append(buf, n);
            std::fclose(f);
        }
        const std::string page = unzip_from(zip, kPillPagePath);
        check(page.size() > 20000, "the game's name tag page comes out of UI.zip");
        for (int pop = 0; pop < 2; ++pop) {
            std::string why;
            const std::string out = with_pill(page, pop != 0, &why);
            check(!out.empty(), pop ? "the popping box goes into the game's page" : "the plain box goes into the game's page");
            if (FILE* f = std::fopen((std::string(argv[2]) + (pop ? "-pop.xaml" : "-plain.xaml")).c_str(), "wb")) {
                std::fwrite(out.data(), 1, out.size(), f);
                std::fclose(f);
            }
        }
    }

    std::printf("%s\n", g_failed ? "SOME CHECKS FAILED" : "all checks passed");
    return g_failed ? 1 : 0;
}
