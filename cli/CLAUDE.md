# DSView CLI -- Claude Code Instructions

This file is read automatically by Claude Code when working in this directory.

## What this directory provides

Seven MCP tools (registered under the `dsview` server) that let Claude
control DreamSourceLab logic analyzers and oscilloscopes over USB:

| Tool | One-line purpose |
|------|-----------------|
| `scan_devices` | List all connected instruments |
| `device_info` | Get model, version, channel count, channel modes, device mode |
| `capture_logic` | Record a logic capture with named channels and trigger |
| `capture_dso` | Record an oscilloscope capture with vdiv, coupling, probe config |
| `signal_summary` | High-level activity report (logic or voltage stats) |
| `decode_capture` | Per-sample waveform and edge list (auto-detects mode) |
| `decode_analog` | Per-sample voltage waveform and statistics for DSO captures |

---

## When to use logic analyzer vs oscilloscope

**Use Logic Analyzer (`capture_logic`)** for:
- Protocol decoding: I2C addresses/data, SPI transactions, UART frames
- Monitoring many signals at once (up to 16 channels)
- Digital state verification: GPIO toggling, interrupt timing, bus arbitration
- Any task where you need **protocol content** (what was sent on the bus)
- Export to `.sr` format for sigrok protocol decoders

**Use Oscilloscope (`capture_dso`)** for:
- Measuring voltage levels, amplitude, Vpp, DC offset
- Analog timing compliance: rise/fall time at VIL/VIH thresholds
- I2C/SPI setup time, hold time, bus frequency from analog waveforms
- Signal integrity: overshoot, undershoot, ringing, noise margin
- RC charge curves on open-drain buses (I2C pull-up characterization)
- Power rail analysis: ripple measurement with AC coupling

**Key distinction**: Protocol decoders (sigrok) work on **digital** signals
from the logic analyzer -- they tell you *what* was communicated. Analog
timing compliance requires the **oscilloscope** -- it tells you *how well*
the electrical signals meet spec (rise/fall time at VIL/VIH, setup/hold
time, signal integrity). For complete bus analysis, use **both** instruments.

---

## Device mode detection

DreamSourceLab instruments operate in one of three modes:

| Mode | `mode_name` | Capture tool | Decode tool |
|------|-------------|--------------|-------------|
| Logic analyzer | `LOGIC` | `capture_logic()` | `decode_capture()` |
| Oscilloscope (buffered) | `DSO` | `capture_dso()` | `decode_analog()` |
| Oscilloscope (streaming) | `ANALOG` | `capture_dso()` | `decode_analog()` |

**Always check `device_info()` first** -- the `mode_name` field tells you
which capture tool to use.  Using the wrong tool will fail.

`decode_capture()` auto-detects DSO files and redirects to `decode_analog()`.

---

## Logic analyzer workflow

```
scan_devices()                 # 1. discover what is plugged in
device_info(device_index=N)    # 2. confirm mode_name='LOGIC', read channel_modes
capture_logic(...)             # 3. record digital signals
signal_summary(file_path)      # 4. quick overview (transitions, frequency)
decode_capture(file_path, ...) # 5. detailed waveform / edge analysis
```

**Never call `capture_logic` before checking `device_info`.**
The `channel_modes` field in `device_info` output is the authoritative
table of how many channels can be active at each maximum samplerate.

---

## Oscilloscope workflow

```
scan_devices()                 # 1. discover instruments
device_info(device_index=N)    # 2. confirm mode_name='DSO', see analog_channels
capture_dso(...)               # 3. record analog waveform
signal_summary(file_path)      # 4. voltage stats (min/max/Vpp/Vrms/freq)
decode_analog(file_path, ...)  # 5. per-sample voltage waveform
```

**Never call `capture_dso` before checking `device_info`.**
The `analog_channels` field shows current vdiv, coupling, and probe settings.

---

## Voltage division and probe factor (DSO)

The oscilloscope uses an 8-bit ADC (0-255).  The voltage conversion formula:

```
full_scale_mV = vdiv_mV * probe_factor * 10   (10 vertical divisions)
voltage_mV = (hw_offset - raw_sample) * full_scale_mV / 255
```

### vdiv (voltage per division)

Sets the vertical scale.  Available options: 10, 20, 50, 100, 200, 500, 1000, 2000 mV.

Choose vdiv so the expected signal fills most of the screen:
- 3.3V logic signal: `vdiv=500` gives +/-2.5V range
- 1.8V signal: `vdiv=200` gives +/-1.0V range
- 12V power rail: `vdiv=2000` with 10x probe gives +/-100V range

```python
capture_dso(vdiv="500", ...)           # both channels at 500 mV/div
capture_dso(vdiv="0:500,1:1000", ...)  # CH0=500, CH1=1000 mV/div
```

### probe_factor

Must match your physical probe attenuation: 1 (1x), 2 (2x), 10 (10x), 20 (20x).

```python
capture_dso(probe_factor="10", ...)          # both channels 10x probe
capture_dso(probe_factor="0:1,1:10", ...)    # CH0=1x, CH1=10x
```

---

## Coupling modes (DSO)

| Mode | Description | Use when |
|------|-------------|----------|
| `DC` | Full signal passes through | Measuring DC levels, digital signals |
| `AC` | DC component is blocked | Measuring AC ripple on a DC rail |

```python
capture_dso(coupling="DC", ...)           # both channels DC
capture_dso(coupling="0:DC,1:AC", ...)    # CH0=DC, CH1=AC
```

---

## DSO trigger modes

| `trigger_type` | Description |
|----------------|-------------|
| `none` | Free-run / auto trigger (default) |
| `rising` | Trigger on rising edge |
| `falling` | Trigger on falling edge |

- `trigger_channel`: 0 or 1 (must be an enabled channel), -1 for auto
- `trigger_pos`: 0-100, percentage of window before trigger (50 = centre)

```python
capture_dso(
    trigger_channel=0,
    trigger_type="rising",
    trigger_pos=10,       # 10% pre-trigger, 90% post-trigger
    ...
)
```

---

## Channel modes (channels vs max samplerate)

The DSLogic U3Pro16 (SuperSpeed USB) channel mode table from `device_info`:

| Active channels | Max samplerate |
|:-:|:-:|
| 16 | 125 MHz |
| 12 | 250 MHz |
| 6  | 500 MHz |
| 3  | 1 GHz   |

Other DreamSourceLab models have different tables -- always read
`channel_modes` from `device_info` for the attached device.

Example: to capture at 500 MHz you must limit to 6 or fewer active channels:
```python
capture_logic(channels="0-5", samplerate="500M", ...)
```

---

## Capture duration

Instead of specifying `num_samples`, you can specify `duration` with
a time suffix.  The sample count is computed automatically from
`samplerate * duration`.

Supported suffixes: `s`, `ms`, `us`, `ns`.

Works with both `capture_logic()` and `capture_dso()`.

```python
# Logic: Capture 100 ms of I2C at 10 MHz
capture_logic(
    channels="0,1",
    channel_names="SDA,SCL",
    samplerate="10M",
    duration="100ms",
)

# DSO: Capture 50 ms at 10 MHz
capture_dso(
    channels="0",
    channel_names="VOUT",
    samplerate="10M",
    duration="50ms",
    vdiv="500",
)
```

When `duration` is provided, `num_samples` is ignored.  The result
includes `requested_duration` and `computed_num_samples` fields.

---

## Multi-device support

When multiple instruments are connected simultaneously:

1. `scan_devices()` returns each with a unique `index`.
2. Pass `device_index=N` to `device_info`, `capture_logic`, or `capture_dso`.
3. Each device is independent -- you can run captures on different devices
   within the same session.

Example with a logic analyzer and an oscilloscope:
```
scan_devices()
# returns [{index:0, name:"DSLogic U3Pro16"}, {index:1, name:"DSCope U3P100"}]

device_info(device_index=0)  # mode_name='LOGIC'
capture_logic(device_index=0, ...)

device_info(device_index=1)  # mode_name='DSO'
capture_dso(device_index=1, ...)
```

---

## Naming channels

Always provide `channel_names` when the signals have meaningful roles.
The names are stored in the `.meta.json` sidecar and appear in all
subsequent analysis tool outputs.

```python
# Logic analyzer
capture_logic(
    channels="0,1,2,3",
    channel_names="SDA,SCL,TX,RX",
    ...
)

# Oscilloscope
capture_dso(
    channels="0,1",
    channel_names="VOUT,GND_REF",
    ...
)
```

Names must be in the **same order** as the channel indices in `channels`.

---

## Voltage threshold (logic analyzer only)

Pro devices (e.g. DSLogic U3Pro16) support variable input voltage
thresholds via `voltage_threshold` in `capture_logic()`.  The default
is 1.0V (set by the device firmware).

Common values:
- `0.8`  -- LVCMOS 1.2V/1.5V signals
- `1.0`  -- default, suitable for 1.8V and above
- `1.2`  -- 2.5V LVCMOS
- `1.5`  -- 3.3V LVCMOS
- `2.5`  -- 5V TTL

Check `device_info()` output for the current `vth` value.

```python
capture_logic(
    channels="0,1",
    channel_names="SDA,SCL",
    voltage_threshold=1.8,
    ...
)
```

---

## Triggers (logic analyzer)

- `trigger_channel` must be one of the physical indices listed in `channels`.
- `trigger_type`: `none` (free-run), `rising`, `falling`, `high`, `low`.
- `trigger_pos` (0-100): percentage of samples captured **before** the
  trigger event.  Default 50 centres the trigger in the window.
  Use a low value (e.g. 10) to see mostly post-trigger data.

---

## Output files

Every `capture_logic` and `capture_dso` call produces two files:

```
<out_file>.bin            raw binary sample data
<out_file>.bin.meta.json  samplerate, channel map, trigger config, mode
```

The metadata sidecar includes a `"mode"` field (`"logic"`, `"dso"`, or
`"analog"`) so analysis tools automatically choose the right decoder.

Pass `out_file` explicitly when you want a predictable path, otherwise a
temp file under `/tmp/dsview_*.bin` is created automatically.

Both `signal_summary` and `decode_capture`/`decode_analog` require the
`.bin` path; they locate the sidecar automatically.

---

## Output format

Tool results are **TOON** (Terse Object-Oriented Notation) by default,
which is more compact than JSON.  Use `toon.decode(result)` in Python
to convert back to a dict when you need to inspect values programmatically.

---

## Export formats

| Format | Logic | DSO | Notes |
|--------|:-----:|:---:|-------|
| `bin` | Yes | Yes | Native binary + .meta.json (default) |
| `csv` | Yes | Yes | Logic: 0/1 values. DSO: voltage in mV with time column |
| `sr` | Yes | Yes | Sigrok session ZIP. DSO: 32-bit float voltages |
| `vcd` | Yes | Yes* | VCD is digital. DSO signals thresholded at midpoint |
| `sigrok-binary` | Yes | Yes | Raw bytes without header |

*DSO VCD export thresholds analog signals at the ADC midpoint to produce
digital waveforms.  Use CSV for actual voltage values.

---

## Protocol decoding with sigrok

For protocol-level analysis (I2C, SPI, UART, etc.), use the sigrok MCP
server's `decode_protocol` tool on exported captures.  The recommended
workflow:

1. Capture with `out_format="sr"` (or `"vcd"`) and meaningful channel names.
2. Use `signal_summary()` to confirm channels have activity.
3. Use sigrok's `decode_protocol()` for protocol analysis.

```python
# Step 1: Capture I2C with .sr export
result = capture_logic(
    channels="0,1",
    channel_names="SDA,SCL",
    samplerate="10M",
    out_format="sr",
)

# Step 2: Decode with sigrok
decode_protocol(
    input_file=result["export_file"],
    protocol_decoders="i2c:scl=SCL:sda=SDA",
)
```

Common protocol decoder strings:
- **I2C**: `i2c:scl=SCL:sda=SDA`
- **SPI**: `spi:clk=CLK:mosi=MOSI:miso=MISO:cs=CS`
- **UART**: `uart:rx=RX:baudrate=115200`
- **1-Wire**: `onewire_link:owr=OWR`

The `capture_logic()` return value includes a `decode_hint` field with
auto-detected protocol suggestions based on channel names.

**Samplerate guidelines for protocol decoding:**
- I2C (100kHz-400kHz bus): 1-10 MHz samplerate
- SPI (1-50 MHz clock): 10-100 MHz samplerate (>= 4x clock frequency)
- UART (9600-115200 baud): 1-10 MHz samplerate
- UART (1-3 Mbaud): 10-50 MHz samplerate

---

## Samplerate optimisation (Nyquist hints)

`signal_summary()` automatically analyses captured signals and provides
samplerate recommendations based on Nyquist sampling theory.  This works
for both logic and DSO captures.

- **Per-channel fields** (when frequency is detected):
  - `min_samplerate_hz` -- 4x measured frequency (absolute minimum)
  - `rec_samplerate_hz` -- 10x measured frequency (recommended)
  - `oversampling_ratio` -- current samplerate / signal frequency
  - `hint` -- textual recommendation when oversampling > 20x

- **Top-level `samplerate_hint`** when the capture is heavily oversampled.

**Recommended workflow for optimising capture duration:**
1. Do an initial capture at a generous samplerate (e.g. 10 MHz)
2. Run `signal_summary()` to see actual signal frequencies
3. Check the `samplerate_hint` -- if oversampled, re-capture at the
   recommended rate to get a proportionally longer capture window
4. Use `device_info()` `capture_budget` table to see max duration at
   the recommended rate

Example: if `signal_summary()` reports an I2C bus at ~400 kHz captured
at 100 MHz (250x oversample), it recommends 4 MHz (10x Nyquist).
Switching to 4 MHz allows 25x longer captures at the same memory depth.

---

## Build / environment

The tools require the compiled `dsview-cli` binary. Build from the
DSView root directory:

```sh
cd /path/to/DSView
mkdir -p build && cd build && cmake .. && make -j$(nproc)
```

This builds both the DSView GUI and `dsview-cli`. The binary is placed
in `build.dir/dsview-cli` alongside the DSView GUI binary.

Firmware files live in `DSView/res/` and are located automatically
at runtime relative to the binary location. The system install path
`/usr/share/DSView/res/` is used as a fallback.

---

## Key implementation files

| File | Role |
|------|------|
| `cli/dsview_mcp.py` | MCP server (Python, FastMCP) -- registered in `~/.claude.json` |
| `cli/dsview_cli.c` | C bridge -- `scan` / `info` / `capture` subcommands, JSON output |
| `cli/CMakeLists.txt` | Builds `dsview-cli`; links against DSView's libsigrok4DSL sources |
| `DSView/res/` | FPGA firmware loaded via libsigrok4DSL on device open |

---

## MCP server registration

Update your `~/.claude.json` to point to the new location:

```json
{
  "mcpServers": {
    "dsview": {
      "command": "python3",
      "args": ["/path/to/DSView/cli/dsview_mcp.py"]
    }
  }
}
```
