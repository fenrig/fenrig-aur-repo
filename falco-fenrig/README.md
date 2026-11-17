# falco-fenrig

Arch Linux package for Falco with initial runtime-security rules for Plasma
desktop applications.

The initial application policy covers Firefox, Chromium, Chrome, Brave,
Vivaldi, Opera, Edge, LibreWolf, Floorp, qutebrowser, Thunderbird, Mumble,
Stremio, Stremio Enhanced, Electron, Ferdium, Discord, WoWUp-CF, Wine,
Evince, Okular, Ark, Gwenview, and Spectacle.
Kate and Dolphin are intentionally excluded because their normal workflows
include editing files, launching scripts, and opening tools.
Konsole is intentionally excluded because launching a shell is its normal
purpose and would generate noisy alerts.

## Build and install

```sh
makepkg -s
sudo pacman -U falco-fenrig-*.pkg.tar.zst
sudo systemctl enable --now falco.service
```

Check alerts with:

```sh
journalctl -u falco.service -f
```

Alerts are also written as JSON to `/var/log/falco/falco.log`. If `logrotate`
is installed, the package rotates this file only when it reaches 256 MiB and
retains four compressed rotations.

The package currently targets x86_64 and builds Falco from its upstream source
with `-march=native -mtune=native`. The resulting package is specific to the
CPU that built it and should not be copied to another machine with a different
CPU feature set.

Review the rules before fleet deployment; application process names and Wine
behavior vary by package and user workload.
