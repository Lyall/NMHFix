# No More Heroes Fix
[![Patreon-Button](https://github.com/user-attachments/assets/73df94d4-a016-4027-bde8-d6862ed1670f)](https://www.patreon.com/Wintermance) [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/W7W01UAI9)<br />
[![Github All Releases](https://img.shields.io/github/downloads/Lyall/NMHFix/total.svg)](https://github.com/Lyall/NMHFix/releases)

This is a fix for No More Heroes that adds ultrawide support and more.

## Features
- Ultrawide aspect ratio support.
- Correctly scaled FMVs.
- Intro skip.

## Installation
- Grab the latest release of NMHFix from [here.](https://github.com/Lyall/NMHFix/releases)
- Extract the contents of the release zip in to the the game folder. (e.g. "**steamapps\common\No More Heroes**" for Steam).

### Steam Deck/Linux Additional Instructions
🚩**You do not need to do this if you are using Windows!**
- Open up the game properties in Steam and add `WINEDLLOVERRIDES="version=n,b" %command%` to the launch options.

## Configuration
- See **NMHFix.ini** to adjust settings for the fix.

## Known Issues
Please report any issues you see.
This list will contain bugs which may or may not be fixed.

- HUD is stretched at ultrawide aspect ratios.
- Aspect ratios narrower than 16:9 are stretched.

## Screenshots

| ![ezgif-5-b6896627b0](https://github.com/user-attachments/assets/541c807a-0fcc-44d5-8f64-dfbed6bcc54c) |
|:--:|
| Gameplay |

## Credits
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) for ASI loading. <br />
[inipp](https://github.com/mcmtroffaes/inipp) for ini reading. <br />
[spdlog](https://github.com/gabime/spdlog) for logging. <br />
[safetyhook](https://github.com/cursey/safetyhook) for hooking.
