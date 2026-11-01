# pacman-updater Arch package

This directory contains an Arch Linux `PKGBUILD` for the upstream
`pacman-updater` project. It builds from the public `main` branch, runs the
upstream CMake tests when a CTest manifest is generated, and installs only
package files; it does not run `install.sh`.

Build in an Arch environment with:

```sh
makepkg --printsrcinfo
makepkg --verifysource
makepkg
```

The package installs the updater binaries in `/usr/bin`, the systemd units in
`/usr/lib/systemd/system`, the configuration in `/etc/pacman-updater.conf`,
and the pacman wrapper in `/usr/lib/pacman-updater/`. The configuration is a
backup file. The timer is shipped disabled; an administrator may enable it
explicitly after reviewing the configuration.

`pacman`, `makepkg`, and `yay` are administrator requirements for the updater
workflow rather than package dependencies. In particular, `yay` must be
installed and configured separately if AUR updates are desired. The
administrator must also configure `aur_user` and create that account; this
package does not create users or grant sudo permissions.

The upstream non-packaged installer may refer to `/usr/local/bin`. The
PKGBUILD makes the minimal equivalent adjustment in the packaged service unit
when that prefix occurs, so the service can locate the Arch-installed
executables in `/usr/bin`; no other service behavior is changed.

After installation, the package prints administrator guidance for the
optional `aur_user` setup and a narrowly scoped wrapper sudoers rule. It does
not create the account or modify sudoers automatically.

The upstream checkout could not be inspected in the current offline build
environment. Therefore the PKGBUILD deliberately fails if any requested
upstream file or executable is absent instead of silently substituting paths.
