QuickChat
=========

Quick chat for Forza Horizon 2 online (Azimuth): pick a short phrase from
ANNA's menu, like "Thanks!" or "Nice race!", to send to the other
players. Messages show in a chat log over the minimap, and another
player's message also pops up in a box over their car. The game's files
are never touched.

Project page and source: https://github.com/Trinculo54/QuickChat


Requirements
~~~~~~~~~~~~

  - Forza Horizon 2 running through Azimuth (from Azimuth 1.0.3, the
    platform is the DWrite.dll in this zip).
  - FH2 ANNA (https://github.com/qiangqiang101/FH2-ANNA), for the menu
    you send from. Without it you can't send, but other players'
    messages still show.
  - An online mod that carries the messages between players: FH2MP.
    Offline, your own messages still show, so you can try it.


Install
~~~~~~~

1. Close Azimuth.
2. Extract this zip into your Azimuth folder (the one with azimuth.exe).
   You should end up with:
       d3dcompiler_47.dll            next to azimuth.exe
       DWrite.dll                    next to azimuth.exe
       fh2-mods\QuickChat.dll
3. Start the game.

d3dcompiler_47.dll and DWrite.dll are the same file: the FH2 Mod
Platform, the shared base for FH2 mods
(https://github.com/Trinculo54/fh2-mod-platform). Azimuth 1.0.3 loads
DWrite.dll, older versions d3dcompiler_47.dll. It loads every mod in
fh2-mods. Each mod's zip ships the platform it was built with, so extract
the zip with the newest platform last.

This zip has platform 2.9.1. QuickChat runs on 2.6.0 or newer; the chat
log over the minimap needs 2.9.0, and the box over a car 2.9.1. If
platform.log says QuickChat "needs FH2 Mod Platform 2.6.0 or newer",
another mod's zip put an older platform back: extract this zip again.


Sending
~~~~~~~

  - Tap d-pad down to open ANNA, go to Quick Chat, and choose a phrase
    with d-pad right. ANNA closes and the phrase is sent.
  - With ANNA's voice commands on: say "ANNA, Quick Chat", then the phrase.
  - One message every 1.5 seconds at most.

Only a phrase's number goes over the network. Every player has the same
16 phrases, so there's no free text, and a number never changes meaning.


Settings
~~~~~~~~

Pause > MODS > YOUR MODS > QuickChat, or fh2-mods\QuickChat.ini:

  Quick chat                          on or off
  Quick chat: other players' messages off: you still send, but other
                                      players' messages don't show
  Quick chat: box over cars           off: messages only show in the chat
                                      log (from the next start)

The box over a car is the game's own name tag page with a box added, so
another mod that replaces that page can't be used with it. FH2MP replaces
it too, for its developer badge, and from its release after 1.5.1 its
page has QuickChat's box as well, so with FH2MP installed QuickChat
leaves the page to FH2MP.


Uninstall
~~~~~~~~~

Delete fh2-mods\QuickChat.dll and fh2-mods\QuickChat.ini. Keep the
platform files if other mods use them.


QuickChat is free software under the GNU GPL v3 (LICENSE.txt). The FH2
Mod Platform is MIT, and includes MinHook (LICENSE-MinHook.txt).
