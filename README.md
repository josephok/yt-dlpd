# yt-dlpd

yt-dlp daemon, monitoring urls in file and invoking yt-dlp download when changed.

## Requirements

- Linux (the only supported platform)
- A C compiler (gcc or clang with GNU17 support)
- [Meson](https://mesonbuild.com/) >= 0.54
- [Ninja](https://ninja-build.org/)

### Installing dependencies

Debian / Ubuntu:

```sh
sudo apt install build-essential meson ninja-build
```

Fedora:

```sh
sudo dnf install gcc meson ninja-build
```

Arch Linux:

```sh
sudo pacman -S base-devel meson ninja
```

Alternatively, install via pip: `pip install meson ninja`.

## Build steps

Run the following from the project root:

1. Configure the build directory (only needed once):

   ```sh
   meson setup build
   ```

2. Compile:

   ```sh
   meson compile -C build
   ```

   This is equivalent to `ninja -C build`.

3. Run:

   ```sh
   ./build/yt-dlpd -a urls.txt
   ```

The resulting binary is `build/yt-dlpd`.

## Common options

Release (optimized) build:

```sh
meson setup build-release --buildtype=release
meson compile -C build-release
```

Optimized build with debug info:

```sh
meson setup build-relwithdebinfo --buildtype=debugoptimized
```

Debug build (the default; can be set explicitly):

```sh
meson setup build --buildtype=debug
```

Change the configuration of an existing build directory:

```sh
meson configure build -Dbuildtype=release
```

Choose a compiler:

```sh
CC=clang meson setup build-clang
```

## Install

```sh
meson install -C build                      # Installs to /usr/local by default; may need sudo
meson setup build --prefix=$HOME/.local     # Or set an install prefix at configure time
```

## Clean

```sh
ninja -C build clean   # Remove build artifacts
rm -rf build           # Delete the build directory entirely
```

## Usage

```sh
yt-dlpd -a urls.txt
yt-dlpd --batch-file urls.txt
```

| Option | Description |
|--------|-------------|
| `-a FILE`, `--batch-file FILE` | **Required.** File containing the URLs to download, one per line. It is monitored for changes. If it does not exist, it is created (an existing file is never truncated). |
| `-h`, `--help` | Show help and exit. |
| `-v`, `--version` | Show version and exit. |

`yt-dlpd` runs in the foreground until interrupted (Ctrl+C or SIGTERM):

1. On startup it prints `[yt-dlpd] Monitoring <file>` and watches the batch file via inotify.
2. When the file is modified, or replaced/removed and recreated (e.g. by an editor), it prints `[yt-dlpd] <file> changed` and `[yt-dlpd] Invoking yt-dlp to download...`, then runs `yt-dlp -a <file>`.
3. It waits for yt-dlp to exit before doing anything else, so yt-dlp is never run concurrently. Changes made while yt-dlp is running trigger another run once it finishes.
4. On success it prints `[yt-dlpd] Download finished`. If yt-dlp fails or cannot be started, an error is printed to stderr and monitoring continues.

`yt-dlp` must be installed and available in `PATH`. To avoid duplicated download, make sure your yt-dlp config(`~/.config/yt-dlp/config`) has following config: `--download-archive ~/archive.txt`

## Run as a systemd service

The repository ships a unit file, [ytdlpd.service](ytdlpd.service), to run `yt-dlpd` in the background.

1. Install the binary somewhere stable, e.g. `cp build/yt-dlpd ~/bin/` (or `meson install -C build`).

2. Edit `ytdlpd.service` before installing it. **These values must be changed for your system:**

   - `User=` / `Group=`: change `joe` to your username (run `whoami` to find out). Also update `Environment="HOME=..."` and `WorkingDirectory=` to your home directory.
   - `ExecStart=`: change it to the **full path** of the `yt-dlpd` binary and the **full path** of the file to monitor:

     ```ini
     ExecStart=/home/youruser/bin/yt-dlpd -a /home/youruser/yt/urls.txt
     ```

3. Install and start it:

   ```sh
   sudo cp ytdlpd.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable --now ytdlpd
   ```

4. Manage and inspect it:

   ```sh
   systemctl status ytdlpd      # Current state
   journalctl -u ytdlpd -f      # Follow the logs
   sudo systemctl restart ytdlpd
   sudo systemctl disable --now ytdlpd
   ```

Notes:

- The directory containing the monitored file must already exist and be writable by `User`; the file itself is created if missing.
- `yt-dlp` must be in the service's `PATH` (systemd's default, e.g. `/usr/bin`). Locations such as `~/.local/bin` are not included.
- If you get a "cannot execute yt-dlp" / command-not-found error, add the directory containing `yt-dlp` (find it with `which yt-dlp`) to `PATH` in the `[Service]` section of `ytdlpd.service`:

  ```ini
  Environment="PATH=/home/youruser/.local/bin:/usr/local/bin:/usr/bin:/bin"
  ```
- yt-dlp reads its config (`~/.config/yt-dlp/config`) from the `HOME` set in the unit.
- After editing the unit file, run `sudo systemctl daemon-reload` and `sudo systemctl restart ytdlpd`.

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.
