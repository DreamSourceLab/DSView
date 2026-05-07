# DSView CLI -- Headless MCP Agent

A headless command-line bridge and [Model Context Protocol][mcp] (MCP) server
for DreamSourceLab USB instruments (DSLogic logic analyzers, DSCope
oscilloscopes).  This lets AI coding assistants such as [Claude Code][cc]
directly control the hardware for automated signal capture, protocol
decoding, and analog timing analysis.

[mcp]: https://modelcontextprotocol.io/
[cc]: https://docs.anthropic.com/en/docs/claude-code

## What is included

| File | Purpose |
|------|---------|
| `dsview_cli.c` | C bridge -- `scan`, `info`, `capture` subcommands (JSON output) |
| `dsview_mcp.py` | Python MCP server wrapping the CLI (7 tools for Claude) |
| `CMakeLists.txt` | Builds `dsview-cli`; links against DSView's libsigrok4DSL |
| `requirements.txt` | Python runtime dependencies |
| `CLAUDE.md` | Context file read automatically by Claude Code |

## Prerequisites

### Build dependencies

Everything the main DSView GUI needs, minus the Qt/Boost/FFTW
libraries (the CLI does not use them):

```
sudo apt install build-essential cmake pkg-config \
    libglib2.0-dev libusb-1.0-0-dev zlib1g-dev python3-dev
```

### Python dependencies

```
pip install -r cli/requirements.txt
```

This installs:

- `mcp` (>= 1.8.1) -- the MCP SDK (FastMCP is bundled inside)
- `python-toon` (>= 0.1.3) -- compact serializer (optional, falls back to JSON)

## Building

From the DSView repository root:

```bash
mkdir -p build && cd build
cmake ..
make dsview-cli -j$(nproc)
```

The binary is placed in `build.dir/dsview-cli` alongside the DSView GUI.

To build everything (GUI + CLI):

```bash
make -j$(nproc)
```

### Verify the build

```bash
./build.dir/dsview-cli scan
```

This should print JSON listing connected DreamSourceLab devices (or an
empty list if none are plugged in).

## Registering the MCP server

### Claude Code

Add the following to `~/.claude.json` (create the file if it does not
exist):

```json
{
  "mcpServers": {
    "dsview": {
      "command": "python3",
      "args": ["/absolute/path/to/DSView/cli/dsview_mcp.py"]
    }
  }
}
```

Replace the path with the actual location of your DSView checkout.
Restart Claude Code after editing the config.

### VS Code

Similarly, open VS Code and press Ctrl+Shift+P (or Cmd+Shift+P on Mac)
Type "Open User Settings (JSON)" and select it Add the following to
your settings.json:

```json
{
  "servers": {
    "dsview": {
      "type": "stdio",
      "command": "python3",
      "args": ["/absolute/path/to/DSView/cli/dsview_mcp.py"]
    }
  }
}
```

### Other MCP clients

Any MCP-compatible client can use the server.  The transport is
**stdio** -- launch `python3 cli/dsview_mcp.py` and communicate over
stdin/stdout using the MCP JSON-RPC protocol.

## Available MCP tools

| Tool | Description |
|------|-------------|
| `scan_devices` | List all connected DreamSourceLab instruments |
| `device_info` | Query model, channels, samplerate limits, analog config |
| `capture_logic` | Record digital signals with named channels and trigger |
| `capture_dso` | Record analog waveforms with vdiv, coupling, probe config |
| `signal_summary` | Per-channel activity report (transitions, voltage stats) |
| `decode_capture` | Per-sample digital waveform and edge list |
| `decode_analog` | Per-sample voltage waveform and statistics |

## Quick start

Once the MCP server is registered, ask Claude Code:

```
> Scan for connected instruments
> Show me the device info for the logic analyzer
> Capture 100ms of I2C on channels 0,1 at 10MHz and decode it
> Capture an analog waveform on channel 0 at 100MHz with 500mV/div
```

### Typical logic analyzer workflow

1. `scan_devices()` -- discover connected instruments
2. `device_info(device_index=N)` -- confirm mode is LOGIC, read channel modes
3. `capture_logic(channels="0,1", channel_names="SDA,SCL", samplerate="10M", out_format="sr")` -- capture
4. `signal_summary(file_path)` -- quick overview
5. Use sigrok for protocol decode: `sigrok-cli -i file.sr -P i2c:scl=SCL:sda=SDA`

### Typical oscilloscope workflow

1. `scan_devices()` -- discover instruments
2. `device_info(device_index=N)` -- confirm mode is DSO
3. `capture_dso(channels="0,1", channel_names="SCL,SDA", samplerate="100M", vdiv="500")` -- capture
4. `signal_summary(file_path)` -- voltage statistics (min/max/Vpp/Vrms)
5. `decode_analog(file_path)` -- per-sample voltage waveform

## When to use logic analyzer vs oscilloscope

**Logic analyzer** (`capture_logic`): protocol decoding (I2C, SPI, UART),
monitoring many digital signals, verifying bus transactions.  Tells you
*what* was communicated.

**Oscilloscope** (`capture_dso`): voltage measurements, rise/fall time,
setup/hold time at VIL/VIH thresholds, signal integrity analysis.  Tells
you *how well* the electrical signals meet spec.

For complete bus analysis, use both instruments simultaneously (they
operate independently via `device_index`).

## Firmware

DSView instruments require FPGA firmware files at runtime.  These are
located automatically from `DSView/res/` (relative to the binary) or
from the system install path `/usr/share/DSView/res/`.

## License

Same as DSView -- GPLv3+.  See the top-level [LICENSE](../COPYING) file.
