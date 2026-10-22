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

## Usage

```sh
yt-dlpd -a urls.txt
yt-dlpd --batch-file urls.txt
```

| Option | Description |
|--------|-------------|
| `-a FILE`, `--batch-file FILE` | **Required.** File containing the URLs to download, one per line. It is monitored for changes. If it does not exist, it is created (an existing file is never truncated). |
| `-h`, `--help` | Show help and exit. |

`yt-dlpd` runs in the foreground until interrupted (Ctrl+C or SIGTERM):

1. On startup it prints `[yt-dlpd] Monitoring <file>` and watches the batch file via inotify.
2. When the file is modified, or replaced/removed and recreated (e.g. by an editor), it prints `[yt-dlpd] <file> changed` and `[yt-dlpd] Invoking yt-dlp to download...`, then runs `yt-dlp -a <file>`.
3. It waits for yt-dlp to exit before doing anything else, so yt-dlp is never run concurrently. Changes made while yt-dlp is running trigger another run once it finishes.
4. On success it prints `[yt-dlpd] Download finished`. If yt-dlp fails or cannot be started, an error is printed to stderr and monitoring continues.

`yt-dlp` must be installed and available in `PATH`. To avoid duplicated download, make sure your yt-dlp config(`~/.config/yt-dlp/config`) has following config: `--download-archive ~/archive.txt`

Exit codes: `0` on normal exit, `1` if the batch file cannot be opened/created or monitoring fails, `2` on invalid or missing arguments.

## Common options

Release (optimized) build:

```sh
meson setup build-release --buildtype=release
meson compile -C build-release
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

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.
