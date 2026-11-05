# QuickChat

Quick chat for Forza Horizon 2 online: hold **LB** and press the **d-pad** to send a short phrase to the other players, like "Thanks!" or "Nice race!". Messages show as a chat log at the left of the screen, over the minimap: yours as "You", theirs with their gamertag. On a platform without the HUD feed (before 2.9.0) they pop up as the game's own HUD notification instead.

A mod for the [FH2 Mod Platform](https://github.com/Trinculo54/fh2-mod-platform) 2.9.0, for its pad layer and HUD feed. Work in progress: it needs an online mod to carry the messages.

## Using it

- **Send:** hold LB and press a d-pad direction. While LB is held, the d-pad is quick chat's: the radio doesn't change station and telemetry doesn't open.
- **Defaults:** Up "Hello!", Right "Thanks!", Down "Sorry!", Left "Nice race!".
- **One message every 1.5 seconds** at most.
- **Offline**, your own messages still show, so you can try the buttons.

## Settings

Pause > Settings > Mods, or `fh2-mods\QuickChat.ini`:

| Row | Key | What it does |
|---|---|---|
| Quick chat | `Enabled` | On or off. |
| Quick chat: other players' messages | `ShowOthers` | Off: you still send, but other players' messages don't pop up. |
| Quick chat button | `Button` | The button held with the d-pad: LB, RB, View or Off. It still does its own job in the game too: LB is the clutch when you drive manual with clutch, RB and View move the camera. |
| Quick chat: Up, Right, Down, Left | `Up`, `Right`, `Down`, `Left` | The phrase on each direction. |

## Phrases

0 Nice race!, 1 Good game, 2 Thanks!, 3 Sorry!, 4 Follow me, 5 Wait up, 6 Race?, 7 Let's cruise, 8 Nice car!, 9 Nice drift!, 10 Watch out!, 11 On my way, 12 Hello!, 13 Bye!, 14 Yes, 15 No.

Only the phrase's number goes over the network. Every player has the same list, so there's no free text, and numbers never change meaning.

## Extra Options

Extra Options reads the controller itself, so the pad layer can't hide the d-pad from it. With its default bindings, LB + d-pad would also work Extra Options' turn signals (left and right, which also replay the radio change on a tap), hazards and photo mode (up) and autodrive (down). Put them on the left stick click held with the d-pad instead, in `FH2_ExtraOptions.ini`:

```
AutodriveXbox=LeftStick+DPadDown
SignalLeftXbox=LeftStick+DPadLeft
SignalRightXbox=LeftStick+DPadRight
HazardsXbox=LeftStick+DPadUp
PhotoModeXbox=LeftStick+DPadUp
```

A tap of the left stick still toggles the headlights.

## For other mods: QuickChat.Bridge

QuickChat sends nothing over the network itself. It publishes `QuickChat.Bridge` (`include/quickchat.h`) for the mod that carries messages online: `latest` gives the newest message this player sent (a number that goes up by one per message, and its phrase), and `deliver` hands over another player's. Each message is shown once per player and number, so it can be sent until the server has it.

## Building

llvm-mingw (clang), with the platform as the `platform` submodule:

```
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build
```

`-DQC_TESTS=ON` also builds `quickchat_test.exe`, the offline test of the phrase, cooldown and pop-up rules.

## Licence

GPL-3.0, see LICENSE.
