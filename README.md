# QuickChat

Quick chat for Forza Horizon 2 online: pick a short phrase from **ANNA's menu** to send to the other players, like "Thanks!" or "Nice race!". Messages show as a chat log at the left of the screen, over the minimap: yours as "You", theirs with their gamertag. On a platform without the HUD feed (before 2.9.0) they pop up as the game's own HUD notification instead.

A mod for the [FH2 Mod Platform](https://github.com/Trinculo54/fh2-mod-platform), with [FH2 ANNA](https://github.com/qiangqiang101/FH2-ANNA) for its menu. Work in progress: it needs an online mod to carry the messages.

## Using it

- **Send:** tap D-pad down to open ANNA, go to **Quick Chat**, and choose a phrase with D-pad right. ANNA closes and the phrase is sent. The ones said most come first: Hello!, Thanks!, Sorry!, Nice race!
- **By voice:** with ANNA's voice commands on, say "ANNA, Quick Chat", then the phrase.
- **One message every 1.5 seconds** at most.
- **A pill over the car:** another player's message also pops up as a white pill over their car, on their name tag, and fades after about two and a half seconds, as in Forza Horizon 5. This needs FH2 Mod Platform 2.9.1 or newer; on an older one messages only show in the chat log. Your own car has no name tag, so your own messages have no pill on your screen.
- **Offline**, your own messages still show, so you can try it.
- **Without FH2 ANNA** there's no way to send; other players' messages still show.

## Settings

Pause > MODS > YOUR MODS > QuickChat, or `fh2-mods\QuickChat.ini`:

| Row | Key | What it does |
|---|---|---|
| Quick chat | `Enabled` | On or off. Off: the Quick Chat list leaves ANNA's menu, and other players' messages don't show. |
| Quick chat: other players' messages | `ShowOthers` | Off: you still send, but other players' messages don't show. |
| Quick chat: pill over cars | `Pill` | Off: messages only show in the chat log. A change takes effect the next time the game starts. |

Only in `QuickChat.ini`, both read when the game starts:

| Key | What it does |
|---|---|
| `PillPop` | 1: the pill pops up and fades away. 0: it simply appears and goes. Try 0 if the pill never shows. |
| `PillTest` | 1: for trying the pill offline. Every 5 seconds a Drivatar near you "says" a phrase, one car after another, and the chat log names it. |

The pill is the game's own name tag page (`UI.zip`, `Scenes/Anthem/AR_OpponentLabel.xaml`) with a pill added, so another mod that replaces that page can't be used together with it.

## Phrases

0 Nice race!, 1 Good game, 2 Thanks!, 3 Sorry!, 4 Follow me, 5 Wait up, 6 Race?, 7 Let's cruise, 8 Nice car!, 9 Nice drift!, 10 Watch out!, 11 On my way, 12 Hello!, 13 Bye!, 14 Yes, 15 No.

Only the phrase's number goes over the network. Every player has the same list, so there's no free text, and numbers never change meaning.

## For other mods: QuickChat.Bridge

QuickChat sends nothing over the network itself. It publishes `QuickChat.Bridge` (`include/quickchat.h`) for the mod that carries messages online: `latest` gives the newest message this player sent (a number that goes up by one per message, and its phrase), and `deliver` hands over another player's. Each message is shown once per player and number, so it can be sent until the server has it.

## Building

llvm-mingw (clang), with the platform as the `platform` submodule:

```
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build
```

`-DQC_TESTS=ON` also builds `quickchat_test.exe`, the offline test of the phrase, list, cooldown, pop-up and pill rules. Given the game's `media\UI.zip` and a name, it also writes the name tag page with the pill, both ways, under that name.

`src/unzip.hpp` has the FH2 Mod Platform's DEFLATE reader (MIT).

`include/fh2anna.h` is FH2 ANNA's interface for other mods, copied from its repository (MIT, qiangqiang101).

## Licence

GPL-3.0, see LICENSE.
