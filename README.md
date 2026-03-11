# SRAL Mudlet Package

Screen Reader Abstraction Library (SRAL) integration for the Mudlet MUD client. Sends all MUD output directly to your screen reader (NVDA, JAWS, Narrator, etc.) and braille display via the [SRAL library](https://github.com/m1maker/SRAL).

## Requirements

- **Mudlet** (Windows, 64-bit)
- **A screen reader** (NVDA, JAWS, Narrator, or any SRAL-supported engine)
- **SRAL.dll** — the SRAL library from [SRAL Releases](https://github.com/m1maker/SRAL/releases) (download the latest stable release, look in `Lib/X64/SRAL.dll`)

## Installation

### Step 1: Install the Mudlet package

1. In Mudlet, go to **Toolbox → Package Manager** (or press `Alt+O`)
2. Click **Install** and select `sral-mudlet.mpackage`
3. Mudlet will import the package — you can close the Package Manager

### Step 2: Add the SRAL library

The package includes `sral_bridge.dll` (the Lua bridge module), but you also need the actual SRAL library:

1. Download the latest SRAL release from [https://github.com/m1maker/SRAL/releases](https://github.com/m1maker/SRAL/releases)
2. Extract the ZIP and find `SRAL.dll` in the `Lib/X64/` folder
3. Copy `SRAL.dll` into your Mudlet package directory:
   ```
   %APPDATA%\Mudlet\profiles\<your-profile>\sral-mudlet\
   ```
   (Replace `<your-profile>` with your Mudlet profile name)

### Step 3: Add screen reader client DLLs (if needed)

SRAL loads screen reader client libraries at runtime. Place them in the same `sral-mudlet` folder:

- **For NVDA**: Download `nvdaControllerClient.dll` (64-bit) from the [NVDA Controller Client releases](https://github.com/nvaccess/nvda/wiki/Connect) or from the NVDA installation directory, and place it in the `sral-mudlet` folder.
- **For JAWS**: No extra DLLs needed (uses COM).
- **For other screen readers**: Generally no extra DLLs needed.

After setup, the `sral-mudlet` folder should contain:
- `sral_bridge.dll` (Lua bridge — installed automatically with the package)
- `SRAL.dll` (SRAL library — you copied this)
- `nvdaControllerClient.dll` (only if using NVDA — you copied this)

### Step 4: Restart or reconnect

Reconnect to your MUD or restart Mudlet. You should see status messages in the console confirming SRAL loaded and which screen reader engine was detected.

## Commands

All commands use the `sral` prefix:

| Command | Description |
|---|---|
| `sral help` | Show all available commands |
| `sral on` / `sral off` | Enable or disable screen reader output |
| `sral toggle` | Toggle screen reader output on/off |
| `sral stop` | Stop current speech immediately |
| `sral pause` / `sral resume` | Pause or resume speech |
| `sral info` | Show SRAL status, current engine, and available engines |
| `sral say <text>` | Speak specific text through the screen reader |
| `sral braille <text>` | Send text to braille display |
| `sral interrupt on/off` | Toggle whether new lines interrupt current speech |
| `sral mode output/speech` | Set output mode (speech+braille or speech only) |
| `sral exclude <pattern>` | Exclude lines matching a Lua pattern from speech |
| `sral unexclude <pattern>` | Remove an exclusion pattern |
| `sral prefix <text>` | Set a prefix for all spoken text (blank to clear) |
| `sral echo on/off` | Toggle status messages in the Mudlet console |
| `sral save` / `sral load` | Save or load your configuration |
| `sral reinit` | Reinitialize the SRAL library |

## How It Works

The package uses a trigger that fires on every line received from the MUD. Each line is processed (ANSI codes stripped, blank lines filtered, exclusion patterns checked) and then sent to your screen reader via SRAL.

Architecture:
- **sral_bridge.dll** (Lua C module) — A bridge that Mudlet's Lua 5.1 loads via `require("sral_bridge")`. It dynamically loads the SRAL library at runtime using `LoadLibrary`/`GetProcAddress`. Named `sral_bridge` (not `sral`) to avoid a filename collision with `SRAL.dll` on case-insensitive Windows filesystems.
- **SRAL.dll** (SRAL library) — The actual screen reader abstraction layer that communicates with NVDA, JAWS, Narrator, etc.

The bridge calls `SetDllDirectoryA` before loading SRAL.dll, which adds the package directory to the Windows DLL search path. This ensures SRAL can find screen reader client DLLs (like `nvdaControllerClient.dll`) placed in the same folder.

## Configuration

Configuration is saved to `%APPDATA%\Mudlet\profiles\<your-profile>\sral_config.lua` and persists across sessions. Use `sral save` to save manually, or it auto-saves when Mudlet exits.

### Options

- **enabled** — Master on/off switch (default: on)
- **interrupt** — Whether new lines interrupt current speech (default: on)
- **use_output** — Use SRAL Output (speech + braille) vs speech only (default: on)
- **strip_colors** — Strip ANSI color codes from text (default: on)
- **strip_blanks** — Skip blank/whitespace-only lines (default: on)
- **prefix** — Text prepended to all speech (default: empty)
- **max_length** — Maximum text length to speak, 0 for unlimited (default: 0)
- **echo_to_console** — Show status messages in the Mudlet console (default: off)
- **excluded_patterns** — List of Lua patterns; matching lines are skipped

## Troubleshooting

### "Failed to load sral_bridge C module"
- Make sure `sral_bridge.dll` (the Lua bridge file, ~100 KB) is in your profile's `sral-mudlet/` folder. It should be installed automatically with the package.

### "Failed to load SRAL library"
- Make sure `SRAL.dll` (the SRAL library, ~180 KB) is in the same `sral-mudlet/` folder. This file must be downloaded separately from the SRAL releases page.

### "SRAL_Initialize returned false"
- No screen reader was detected. Make sure NVDA, JAWS, or another supported screen reader is running before connecting in Mudlet.

### SRAL only speaks via SAPI (not using NVDA/JAWS)
- **NVDA**: Make sure `nvdaControllerClient.dll` (64-bit) is in the `sral-mudlet/` folder alongside `SRAL.dll`. Without it, SRAL cannot communicate with NVDA.
- **JAWS**: JAWS uses COM, so no extra DLLs are needed. If JAWS is detected but not speaking, try restarting both JAWS and Mudlet.
- Use `sral info` to check which engine is currently selected and which are active.

### Package loads but no speech
- Check `sral info` to see the detected engine
- Try `sral say Hello` to test speech directly
- Make sure your screen reader is running and not muted

## Supported Screen Readers

Via SRAL, this package supports:
- NVDA
- JAWS
- Microsoft Narrator
- ZDSR
- SAPI (Windows built-in speech)
- Speech Dispatcher (Linux)
- VoiceOver (macOS)

## License

This package is provided as-is. SRAL is developed by [m1maker](https://github.com/m1maker/SRAL) — see its repository for licensing details.
