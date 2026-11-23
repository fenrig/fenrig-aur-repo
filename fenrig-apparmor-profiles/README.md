# fenrig-apparmor-profiles

Experimental Arch package for restrictive AppArmor policies covering native
Firefox, Thunderbird, LibreOffice, Evince, Gwenview, Okular, Ark, Spectacle,
Chromium, Google Chrome, Cromite, WoWUp-CF, Mumble, Transmission Qt, Discord, and a specific system
Electron runtime, plus mpv with its yt-dlp YouTube helper and Lutris, on KDE
Plasma, especially Plasma Wayland.
It also installs broad compatibility profiles for Steam/Proton and Wine.

GPU support

The browser, document, Electron, mpv, Lutris, Steam, and Wine policy families
include Arch's Mesa and Vulkan abstractions. The package additionally covers
DRM enumeration and NVIDIA UVM/caps device nodes used by some newer drivers.
This supports the normal AMD, Intel, and NVIDIA render paths, subject to the
usual Unix device ownership and driver installation. It does not grant access
to arbitrary character devices or disable the applications' own sandboxes.
Hardware acceleration still needs to be verified on each target GPU/driver;
missing GPU denials should be investigated before adding local rules.

## Scope and design

The policies retain Firefox/Thunderbird's own sandbox and permit the standard
Wayland, audio/video, fonts, Mesa/Vulkan, CUPS, D-Bus, dconf, KDE, portal/file
chooser and DNS resources through AppArmor abstractions. Persistent access is
limited to the relevant Mozilla profile, Thunderbird mail profile, explicit
application state, non-hidden user-owned home files, and mounted-storage
locations. Browser file saving remains available throughout the normal
non-hidden home directory; hidden dot-file state is granted only where the
application needs it. SSH/GPG/cloud credential directories and shell startup
files remain denied. The policies do not claim
to protect cookies, saved credentials, or mail stores from their owning
application.

The shared sandbox abstraction applies the common user-namespace mapping,
Wayland proxy, bubblewrap/Glycin, hardware-probing, and overflow-ID reads to
Firefox, Thunderbird, Chromium/Chrome/Electron-family applications, and the
document/media policies. It does not grant shared administrative or network
capabilities; compatibility capabilities remain in the specific profiles that
require them. Application-specific data rules remain separate.

Bubblewrap is entered through the package-owned, unattached
`fenrig-bwrap` profile rather than a competing attachment to `/usr/bin/bwrap`.
Its setup domain necessarily has broad namespace, mount, IPC, and capability
permissions because bwrap constructs the sandbox itself. Once setup is
complete, bwrap stacks child processes with `fenrig-bwrap-unpriv`, which audits
and strips the setup capabilities. This keeps the broad setup policy out of
the browser, Thunderbird, and document profiles, but it remains experimental:
verify the resulting labels and sandbox behavior on the pilot machine, and do
not add a second global `/usr/bin/bwrap` attachment from another package.

Covered applications may read ordinary user-owned home-directory contents for
desktop indexing, archive inspection, file pickers, and helper discovery.
Hidden dot-files are not covered by that common rule; application profiles and
shared KDE state abstractions explicitly grant only the dot-directories and
files required for that application. Write access remains profile-specific,
with browsers retaining non-hidden home save/download access.

All packaged profiles also include a read-only runtime abstraction. It permits
same-user `/proc/<pid>` inspection, `/proc/net/if_inet6`, cgroup `cpu.max` and
`memory.max`, and hugepage metadata. These are runtime and resource-discovery
interfaces; no cgroup control or process-credential access is granted. The
LibreOffice policy additionally covers its installed startup/printing files,
`/etc/paperspecs`, the observed Thunderbird profile metadata, and the named
printing/crypto helper programs under the existing LibreOffice confinement.
Electron-family profiles include the standard local CUPS client abstraction.
Chromium-family profiles also permit their own cache root and the user-owned
NSS certificate database at `~/.pki/nssdb`; this is required for browser
certificate and client-authentication support. NVIDIA device-symlink cleanup
attempts remain denied.

AppArmor mediates canonical paths rather than the symlink spelling used by the
desktop. The current pilot machine maps `~/Downloads` to
`/mnt/multi/Downloads` and `~/Music` to `/mnt/multi/Music`; those observed
roots are included for the document/media policies. Other machines with
different mounts should add their canonical paths through
`local/fenrig-document-common`.
Common Dutch XDG names are also covered under `/mnt/multi`: `Documenten`,
`Muziek`, `Afbeeldingen`, `Video's`, `Bureaublad`, and `Openbaar`. The
document/media policies likewise read and write the full home-directory trees
`~/Afbeeldingen`, `~/Video's`, `~/Documenten`, and `~/Muziek` alongside the
English `Pictures`, `Videos`, `Documents`, and `Music` names. Spectacle can
save to both `~/Pictures/Screenshots` and `~/Afbeeldingen/Screenshots`, and
record to `~/Videos/Screencasts` or `~/Video's/Screencasts`.

The application profiles also permit read/write access below `/mnt/` and
`/run/media/` for user-mounted disks and removable media. This supports normal
copy, save, export, upload, and game-library operations across the covered
applications. It is deliberately broader than the home-directory rules, so
do not mount sensitive material there while an untrusted application is open;
Unix ownership, mount options, and desktop automount policy still apply.

Tablet and HID support is enabled for document/media applications, Electron,
Lutris, Steam/Proton, and Wine. The shared tablet abstraction covers the
libinput event nodes, tablet sysfs metadata, the installed `libwacom` database,
and `/dev/hidraw*` for tablet control interfaces. AppArmor path rules cannot
identify a Wacom device by vendor at open time, so this also exposes any raw
HID node visible to the confined process; it is intentionally not included in
the browser abstraction. Raw HID can include keyboards, controllers, and
security tokens. Remove the tablet include from the relevant profile or use
`local/fenrig-tablet-common` only with a site-specific device strategy if that
tradeoff is unacceptable. Normal Wayland browser pen input remains
compositor-mediated and does not need raw HID access.

All desktop application families include the KDE state abstraction. It permits
directory-level read/write operations on `~/.config` for atomic configuration
updates and explicit read/write access to `~/.config/kioslaverc`; it does not
grant recursive access to every configuration file. The ClamAV native host is
intentionally excluded.

Browser profiles also include the Plasma Browser Integration abstraction. It
allows the integration host to run and grants read access to
`~/.config/kcminputrc`, `~/.config/kdedefaults/kcminputrc`, the read-only
`~/.config/menus/` tree, plus direct `~/.cache/ksycoca*` cache and lock
entries. It does not grant recursive access to the rest of `.config` or
`.cache`.

The ClamAV native-messaging host is entered with an AppArmor transition. Its
policy permits the installed host, required libraries/configuration, inherited
stdio, `/run/clamav/clamd.ctl`, read/write access to the per-user
`~/.local/share/clamav/quarantine` and `~/.local/state/clamav` metadata paths,
and read/write access to user-owned home and mounted-storage files for
quarantine and explicit deletion, with credential/key and shell startup
denials, and a narrowly named temporary staging pattern. It has no
shell or arbitrary helper execution rule. Its explicit `open_quarantine`
transition enters a separate `fenrig-clamav-quarantine-gui` profile for
`/usr/bin/clamav-quarantine-gui`; that viewer can inspect, restore, and delete
quarantined payloads without inheriting arbitrary execution access to the
original download or mail roots. Its per-user ClamAV configuration and cache
directories under `~/.config/clamav` and `~/.cache/clamav` are explicit, so Qt
settings and quarantine UI state can be created without recursive access to
unrelated configuration. The quarantine viewer can restore to user-owned home
and removable-storage destinations; credential/key stores and shell startup
files remain denied. The repository
inspection confirmed the manifest path and the configured LocalSocket; if the
scanner implementation changes, update the policy from observed behavior
rather than accepting every `aa-logprof` suggestion.

Firefox and Thunderbird also include the optional Belgian eID abstraction when
Arch `eid-mw` is installed. It permits the observed PKCS#11 module, its
p11-kit/Mozilla registration, and the `/run/pcscd/pcscd.comm` client socket.
The card reader remains behind `pcscd`; browser profiles do not receive
arbitrary USB or raw smart-card device access. Test certificate login and
signing with a real reader and card on the pilot machine.

The installed Arch `apparmor` package ships `firefox` and `thunderbird`
profiles with `flags=(unconfined)`. This package intentionally uses the same
profile names in separate package-owned files and reloads them with
`apparmor_parser --replace`; it never overwrites those files. The hook makes
replacement visible and does not unload policy after a failure. An existing
Firefox or Thunderbird process must be restarted after first attachment.

Optional administrator additions belong in:

```text
/etc/apparmor.d/local/fenrig-firefox
/etc/apparmor.d/local/fenrig-thunderbird
/etc/apparmor.d/local/fenrig-clamav-native
/etc/apparmor.d/local/fenrig-clamav-quarantine-gui
/etc/apparmor.d/local/fenrig-xdg-open
```

The same naming pattern applies to the added applications, for example
`local/fenrig-okular` or `local/fenrig-libreoffice`. These paths are not owned
by the package and survive upgrades. Review every
addition; do not paste a complete home-directory or arbitrary-execution rule.
For mpv, use `local/fenrig-mpv` for narrowly justified mpv helper programs or
`local/fenrig-mpv-yt-dlp` for yt-dlp-specific additions.
Lutris-specific allowances belong in `local/fenrig-lutris`.

## Build and install

From this directory:

```sh
makepkg --verifysource
makepkg --check
makepkg -f
namcap fenrig-apparmor-profiles-*.pkg.tar.zst   # if namcap is installed
sudo pacman -U fenrig-apparmor-profiles-*.pkg.tar.zst
```

After restarting applications on a target machine, run the installed
non-invasive diagnostic:

```sh
/usr/share/doc/fenrig-apparmor-profiles/tests/runtime-check.sh
sudo /usr/share/doc/fenrig-apparmor-profiles/tests/runtime-check.sh \\
  --since '15 minutes ago'
```

The check does not load or unload policy, change enforcement mode, launch
applications, or modify files. It reports loaded profile labels, bwrap child
labels, recent denials, and—when run as root—read-only negative probes through
`aa-exec -p firefox`. A reported denial is a review item rather than an
automatic reason to add a rule. Use the manual pilot matrix below for actual
downloads, portals, media, mail, printing, GPU, scanner, Steam, Wine, and
Lutris functionality.

Installation does not enable AppArmor, change the kernel command line, change
the bootloader, or enable a systemd unit. Preserve the machine's existing LSM
configuration and activate AppArmor separately according to the target's
deployment policy. The pacman hook reloads these profiles only if
`/sys/kernel/security/apparmor/profiles` is active. A failed reload is
reported; protection is not silently disabled.

### Activating AppArmor with systemd-boot

The package prints these instructions after installation, but does not change
boot or service state automatically:

```sh
systemctl status apparmor.service
cat /sys/kernel/security/lsm
cat /proc/cmdline
sudo systemctl enable --now apparmor.service
```

If `apparmor` is not present in `/sys/kernel/security/lsm`, inspect the active
systemd-boot entry and add it to the existing `lsm=` kernel option while
preserving the other configured LSMs. Do not replace the whole option based on
this package's example or remove existing security modules. Reboot, then
verify:

```sh
cat /sys/kernel/security/lsm
sudo aa-status
```

For the current XanMod installation used to build this package, the running
kernel is `7.3.4-x64v3-xanmod1`, AppArmor is absent from the active LSM list,
and the active entry is `/boot/loader/entries/xanmod.conf`. The example edit is
to append this to its existing `options` line:

```text
lsm=landlock,lockdown,yama,integrity,apparmor,bpf
```

Preserve all existing root, initrd, graphics, and other kernel options. Verify
the actual target before using this example; kernel builds and LSM defaults can
differ.

Arch's documented example is
`lsm=landlock,lockdown,yama,integrity,apparmor,bpf`; the correct value depends
on the target kernel and existing security configuration. See the
[ArchWiki AppArmor guidance](https://wiki.archlinux.org/title/AppArmor) and
[systemd-boot guidance](https://wiki.archlinux.org/title/Systemd-boot) before
editing the loader entry.

## Non-invasive validation

The parser check compiles policy without loading it into the kernel:

```sh
apparmor_parser --skip-kernel-load --skip-read-cache --abort-on-error \
  -I /etc/apparmor.d -Q /etc/apparmor.d/fenrig-firefox
apparmor_parser --skip-kernel-load --skip-read-cache --abort-on-error \
  -I /etc/apparmor.d -Q /etc/apparmor.d/fenrig-thunderbird
apparmor_parser --skip-kernel-load --skip-read-cache --abort-on-error \
  -I /etc/apparmor.d -Q /etc/apparmor.d/fenrig-bwrap
apparmor_parser --skip-kernel-load --skip-read-cache --abort-on-error \
  -I /etc/apparmor.d -Q /etc/apparmor.d/fenrig-clamav-native
for profile in fenrig-libreoffice fenrig-evince fenrig-gwenview \
  fenrig-okular fenrig-ark fenrig-spectacle fenrig-mpv; do
  apparmor_parser --skip-kernel-load --skip-read-cache --abort-on-error \
    -I /etc/apparmor.d -Q "/etc/apparmor.d/$profile"
done
```

`aa-status` or `aa-enabled` only establishes activation. Enforcement requires
the intended profile to be loaded and the application restarted:

```sh
sudo aa-status
sudo apparmor_parser -r /etc/apparmor.d/fenrig-firefox
sudo apparmor_parser -r /etc/apparmor.d/fenrig-thunderbird
sudo journalctl -k -g 'apparmor="DENIED"|apparmor' -f
```

Complain mode is useful for diagnosis but does not prove enforcement, and
explicit deny rules can still block operations. Do not automatically trust
all `aa-logprof` proposals; tie allowances to a reproduced feature.

## Target-machine pilot checklist

Run this on one unmanaged pilot machine, with fresh application restarts, and
record profile labels from `aa-status` and `/proc/$PID/attr/current`:

1. Start Firefox, Thunderbird, LibreOffice, Gwenview, Okular, Ark, Spectacle,
   mpv, Transmission Qt, and Lutris; start Evince if installed. Verify the main process and
   plugin/helper children remain functional and retain their built-in sandbox.
   For applications using bwrap/Glycin, record whether helpers show
   `fenrig-bwrap` or `fenrig-bwrap-unpriv` in `/proc/$PID/attr/current` and
   confirm that the application sandbox still reports enabled.
   Start Ferdium separately and verify its renderer/GPU processes show the
   `ferdium` label, services remain usable, and state persists after restart.
2. Exercise DNS, HTTPS browsing, mail send/receive, authentication, and
   certificate/token access.
3. Test Downloads, uploads through the Plasma chooser, mail attachments,
   saving attachments, and opening HTTP(S)/mailto links and a deliberately
   selected attachment. External viewers are outside these profiles and must
   have their own policy or remain unconfined by the desktop policy.
4. Verify Plasma Wayland rendering, clipboard, audio/video, camera/microphone,
   printing, notifications, portals/file chooser, and KDE browser integration.
5. For mpv, play a local file and a YouTube URL, verify Lua scripts under
   `~/.config/mpv/scripts` load, verify yt-dlp starts under the separate
   `fenrig-mpv-yt-dlp` profile,
   test subtitles/cache/screenshots, and confirm unrelated helper execution is
   denied unless explicitly added through a local include.
6. With ClamAV running, scan a harmless test file through both extensions and
   verify the host process has label `fenrig-clamav-native` and connects only
   to `/run/clamav/clamd.ctl`. Open the quarantine UI from each extension and
   verify the viewer has label `fenrig-clamav-quarantine-gui`, can list and
   delete a quarantined test item, and cannot execute or restore it without an
   explicit user-selected destination. Test an EICAR file only in an approved
   isolated test directory.
7. Start Transmission Qt, add a legal test torrent, verify tracker/DHT traffic,
   download into `Downloads` and a `/mnt` or `/run/media` directory, then test
   opening the containing folder. Confirm its state remains limited to the
   Transmission configuration/cache trees and that unrelated credentials are
   inaccessible.
8. Start Steam, update the client, launch one representative Proton game,
   exercise its controller/audio/graphics path, and verify pressure-vessel,
   shader compilation, Steam libraries, and the overlay. Confirm the game
   process remains under the intended Steam profile.
9. Launch one representative Wine application from a dedicated prefix, test
   install/update, file open/save, network, audio, and graphics. Confirm the
   Wine application and `wineserver` labels, and verify the prefix contains no
   host credentials.
10. From a process actually carrying the Firefox/Thunderbird label, test that
   unrelated `~/.ssh`, `~/.gnupg`, `~/.aws`, `~/.kube`, `~/.xonshrc`, shell startup files,
   and unrelated `~/.config`/`~/.local/share` persistence are denied. A normal
   host shell test is not evidence of confinement.
11. Repeat the negative tests from Steam/Proton, Wine, mpv/yt-dlp, and Lutris
   game processes, then review
   denials with `journalctl -k`, `aa-status`, and the process labels.
   Add only a narrowly scoped local rule, reload, restart the app, and repeat.

The current environment has the parser and installed policy files, but its
AppArmor filesystem is not mounted and no target desktop session is available;
therefore parser validation is possible, while kernel enforcement, Plasma
portals, application sandboxes, mail flows, and scanner end-to-end behavior
remain unverified here. Treat this release as experimental until the pilot
passes.

The additional applications are intentionally conservative: archive helper
executables, LibreOffice macros/embedded objects, PDF/office conversion
helpers, and external document viewers may need separately reviewed child
profiles or local rules. No blanket `x` rule is included for those helpers.
If a feature requires one, record the exact executable and transition in the
pilot results before adding it.

The document/media abstraction includes the exact KDE Frameworks 6 KIO worker
and `kioexec` paths with inherited execution. This is required for Gwenview,
Okular, and related KDE applications to create KIO workers; it does not grant
arbitrary helper execution. It also permits the narrow KDE local Unix IPC and
desktop-state paths needed by the tags worker and file chooser, including
thumbnail cache updates, `user-places.xbel` atomic replacement, and linking
the temporary `*.kioworker.socket` endpoint under `/run/user/$UID`. KIO’s
anonymous `#*` socket targets are also limited to that same runtime directory.

Chromium, Google Chrome, and Cromite use separate exact executable attachments
with a shared Chromium-family base. This Cromite build uses the existing
`~/.config/chromium` and `~/.cache/chromium` trees, so its policy grants
owner-scoped read/write access to those complete browser state trees; Cromite
and Chromium are therefore not data-isolated from each other. Chromium and
Google Chrome replace Arch's package-provided unconfined
profile names without overwriting those files. WoWUp-CF and Discord are covered by
their application-specific bundled executable paths. Generic Electron is not
treated as one universal policy: the included `electron` profile targets only
the installed `/usr/lib/electron44/electron` ABI and applies to every app
using that runtime. A future Electron version or an app-bundled runtime needs
a new exact profile and validation.

Ferdium has a separate `ferdium` profile attached to the packaged
`/usr/bin/ferdium` launcher. The observed Arch launcher executes Electron 43
with `/usr/lib/ferdium`; the policy permits that runtime and app tree,
Ferdium's own `~/.config/Ferdium` state, and the shared Electron desktop
integrations. It does not attach to `/usr/bin/electron43` globally, so another
application that invokes Electron 43 directly is not automatically covered.
A future Ferdium package that changes its launcher, Electron ABI, or state
directory requires a profile update and runtime validation.

WoWUp-CF's bundled Electron build currently requests `CAP_SYS_ADMIN`,
`CAP_SYS_CHROOT`, and `CAP_SYS_PTRACE` while
initializing its Chromium sandbox. The package grants that capability only to
the `wowup-cf` profile so that it does not become a permission of the shared
Electron abstraction or unrelated Electron applications. This is a deliberate
compatibility trade-off and weakens the WoWUp profile relative to a build that
does not make that request; revisit it when the WoWUp/Electron bundle changes.
The bundle also invokes `/bin/sh` to derive its machine identifier, so Bash is
executed inside the WoWUp confinement rather than transitioned to unconfined.
The shared Electron abstraction also permits read-only access to `/proc/` and
the kernel inotify watch limit, which are startup/runtime introspection only.

Mumble has explicit TCP/UDP network and audio allowances, but its game
integration plugins may legitimately need access to game processes or memory;
those are not granted by this profile.

mpv has explicit Wayland/audio/video/GPU and network access, read-only access
to normal media directories, and write access only to its configuration,
cache, yt-dlp cache, downloads, and configured screenshot directory. Lua is
embedded in mpv, so Lua scripts are read from the mpv configuration tree and
run inside the mpv profile; they do not receive arbitrary shell execution.
YouTube playback transitions only `/usr/bin/yt-dlp` into a separate restricted
child profile with Python libraries and network access. A Lua extension that
requires another helper must be reviewed and added through a local include.

Lutris is intentionally very broad for game launchers and runtimes: it can
read, write, and execute user-owned files anywhere in `HOME`, as well as use
the removable-storage roots. This accommodates arbitrary game prefixes,
launchers, caches, and runtime helpers. Explicit denies still protect
SSH/GnuPG/cloud credentials, password stores/keyrings, browser/mail profiles,
private `.pem`/`.key` files, and shell startup/configuration files. Treat
Lutris games and downloaded runners as able to access all other user data.
Its exact `pgrep`, `gcc`, `ld`, and `ldconfig` helper allowances support
startup/runtime probing and inherit the Lutris profile.
profiles, selected password-manager configuration, or shell configuration.
This does not protect secrets placed inside a game prefix or Lutris state.
Game-specific helpers not covered by the explicit launcher/runtime paths need
an observed, narrowly scoped `local/fenrig-lutris` rule.

Steam remains constrained to its approved runtime/game trees. Wine is very
broad: it can read, write, and execute user-owned files across `HOME`, `/mnt`,
and `/run/media`, allowing arbitrary prefixes and Windows applications. Wine
still denies common host secrets, keyrings/password stores, browser/mail
profiles, shell startup files, private `.pem`/`.key` files, system writes, and
boot writes. Do not put secrets inside a Wine prefix; AppArmor cannot protect
data legitimately placed inside that prefix. These profiles are installed and
loaded by default when AppArmor is active, but must still pass the pilot matrix
before fleet rollout.

## Removal and recovery

Package removal does not delete the package-owned upstream AppArmor files.
If a loaded profile must be removed, run the commands printed by the package's
removal message, then explicitly reload the original distro profiles that are
present (`firefox`, `thunderbird`, `chromium`, `chrome`, `Discord`, and
`steam`). If a replacement fails, leave the existing loaded policy in place,
inspect parser output, correct the local include or package version, and
retry; never recover by unloading all AppArmor protection.
