#!/usr/bin/env python3
"""DSView MCP Server

Exposes DreamSourceLab instruments (DSLogic, DScope) as MCP tools so that
Claude can scan devices, capture logic-analyzer data, and inspect signals.

Build dsview-cli first (from the DSView root):
  mkdir -p build && cd build && cmake .. && make -j$(nproc)
"""

import json
import math
import os
import struct
import subprocess
import tempfile
import zipfile
from pathlib import Path

try:
    import toon
    _HAS_TOON = True
except ImportError:
    _HAS_TOON = False

from mcp.server.fastmcp import FastMCP

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).parent.resolve()
DSVIEW_ROOT = SCRIPT_DIR.parent

# Search for the dsview-cli binary in likely build output locations
_CLI_CANDIDATES = [
    # in-tree build (EXECUTABLE_OUTPUT_PATH)
    DSVIEW_ROOT / "build.dir" / "dsview-cli",
    DSVIEW_ROOT / "build" / "cli" / "dsview-cli",   # out-of-source: build/cli/
    DSVIEW_ROOT / "build" / "dsview-cli",            # out-of-source: build/
    # system install (Debian package)
    Path("/usr/bin/dsview-cli"),
    Path("/usr/local/bin/dsview-cli"),                # local install
]

CLI_BINARY = None
for _candidate in _CLI_CANDIDATES:
    if _candidate.exists():
        CLI_BINARY = _candidate
        break
if CLI_BINARY is None:
    CLI_BINARY = _CLI_CANDIDATES[0]  # default for error messages

mcp = FastMCP(
    "dsview",
    instructions=(
        "Control DreamSourceLab USB logic analyzers (DSLogic) and oscilloscopes (DScope). "
        "Multiple devices can be connected simultaneously; use device_index to select.\n\n"
        "== When to Use Logic Analyzer vs Oscilloscope ==\n\n"
        "Use LOGIC ANALYZER (capture_logic) when:\n"
        "- Decoding digital protocols: I2C, SPI, UART, JTAG, 1-Wire, SDIO\n"
        "- Checking bus transactions: addresses, data bytes, ACK/NACK, framing\n"
        "- Monitoring many signals simultaneously (up to 16 channels)\n"
        "- Verifying digital state machines, GPIO toggling, interrupt timing\n"
        "- You need protocol-level decoding via sigrok (export .sr format)\n"
        "- The signals are digital (0/1) and you only care about logic states\n\n"
        "Use OSCILLOSCOPE / DSO (capture_dso) when:\n"
        "- Measuring analog signal characteristics: voltage levels, amplitude, Vpp\n"
        "- Measuring timing at specific voltage thresholds (rise/fall time,\n"
        "  setup/hold time, propagation delay)\n"
        "- Checking signal integrity: overshoot, undershoot, ringing, noise\n"
        "- Verifying I2C/SPI timing compliance against spec (requires analog\n"
        "  waveforms to measure at VIL/VIH thresholds, not just logic 0/1)\n"
        "- Measuring AC ripple on power rails (use AC coupling)\n"
        "- Characterizing RC rise times on open-drain buses (I2C, 1-Wire)\n"
        "- You need actual voltage measurements, not just protocol content\n\n"
        "KEY DISTINCTION: Protocol decoders (sigrok) work on digital signals\n"
        "from the logic analyzer. Analog timing compliance (rise/fall time at\n"
        "VIL/VIH, setup/hold time, signal integrity) requires the oscilloscope.\n"
        "For a complete bus analysis, use BOTH: logic analyzer for protocol\n"
        "content, oscilloscope for electrical timing compliance.\n\n"
        "== Logic Analyzer Workflow (DSLogic) ==\n"
        "1. scan_devices      -- find all plugged-in devices (index, name, handle)\n"
        "2. device_info       -- learn channel count, samplerate limits, channel modes\n"
        "                        channel_modes shows channels-in-use vs max samplerate\n"
        "3. capture_logic     -- record digital signals with named channels and trigger\n"
        "4. signal_summary    -- quick overview of which channels are active\n"
        "5. decode_capture    -- view per-channel waveform + edge list\n\n"
        "== Oscilloscope Workflow (DScope) ==\n"
        "1. scan_devices      -- find all plugged-in devices\n"
        "2. device_info       -- check mode_name='DSO', see analog_channels with\n"
        "                        vdiv/coupling/probe_factor, and vdiv_options\n"
        "3. capture_dso       -- record analog waveforms (configure vdiv, coupling, probe)\n"
        "4. signal_summary    -- voltage statistics: min/max/Vpp/Vrms/DC offset/frequency\n"
        "5. decode_analog     -- per-sample voltage waveform and statistics\n\n"
        "Output format: TOON (compact) when available, else JSON.\n\n"
        "Best practices:\n"
        "- ALWAYS call device_info() before capture to check channel_modes and device mode.\n"
        "  mode_name='LOGIC' -> use capture_logic(), mode_name='DSO' -> use capture_dso().\n"
        "- Use meaningful channel_names -- they propagate to all exports and analysis.\n"
        "- For protocol decoding (I2C, SPI, UART, etc.), export with out_format='vcd' or\n"
        "  out_format='sr', then use sigrok's decode_protocol tool.\n"
        "- For analog timing analysis (rise/fall time, setup/hold time, signal integrity),\n"
        "  use capture_dso + decode_analog. Process voltage waveforms with threshold\n"
        "  crossings at VIL/VIH to extract timing parameters.\n"
        "- Set voltage_threshold to match target logic levels (capture_logic only).\n"
        "- For DSO: set vdiv to match expected signal amplitude. The full-scale range\n"
        "  is vdiv * probe_factor * 10 divisions. Use decode_analog() to see voltages.\n"
        "- DSO coupling: DC passes the full signal, AC blocks the DC component.\n"
        "- DSO probe_factor: set to match your probe (1x, 10x, etc.).\n"
        "- Use fewer channels for higher samplerates (see channel_modes).\n"
        "- signal_summary() works for both logic and DSO captures, providing\n"
        "  appropriate metrics for each mode.\n\n"
        "Samplerate selection guide (Nyquist + practical oversampling):\n"
        "  The minimum samplerate is 2x the signal frequency (Nyquist theorem), but\n"
        "  protocol decoders need enough samples per bit to resolve edges reliably.\n"
        "  Use 4x the bus clock as the MINIMUM; 8-10x is recommended when possible.\n"
        "  Higher oversampling wastes capture memory, reducing the observable duration.\n"
        "  ALWAYS pick the LOWEST samplerate that gives reliable decoding.\n\n"
        "  Protocol        Bus clock/baud  Min samplerate  Recommended   Max duration*\n"
        "  I2C standard    100 kHz         400 kHz         1 MHz         ~16 s\n"
        "  I2C fast        400 kHz         1.6 MHz         4 MHz         ~4 s\n"
        "  I2C fast+       1 MHz           4 MHz           10 MHz        ~1.6 s\n"
        "  SPI 1 MHz       1 MHz           4 MHz           10 MHz        ~1.6 s\n"
        "  SPI 10 MHz      10 MHz          40 MHz          100 MHz       ~160 ms\n"
        "  SPI 25 MHz      25 MHz          100 MHz         250 MHz       ~64 ms\n"
        "  UART 9600       9.6 kHz         38.4 kHz        100 kHz       ~160 s\n"
        "  UART 115200     115.2 kHz       460 kHz         1 MHz         ~16 s\n"
        "  UART 1 Mbaud    1 MHz           4 MHz           10 MHz        ~1.6 s\n"
        "  1-Wire          16 kHz          64 kHz          500 kHz       ~32 s\n"
        "  JTAG 10 MHz     10 MHz          40 MHz          100 MHz       ~160 ms\n"
        "  SDIO 25 MHz     25 MHz          100 MHz         250 MHz       ~64 ms\n"
        "  (* max duration assumes 16M sample device memory; check device_info)\n\n"
        "  IMPORTANT: signal_summary() returns per-channel Nyquist-based hints:\n"
        "  min_samplerate_hz (4x), rec_samplerate_hz (10x), oversampling_ratio,\n"
        "  and a top-level samplerate_hint when oversampling is excessive (>20x).\n"
        "  Use these hints to re-capture at an optimal rate for longer duration.\n\n"
        "DSO voltage conversion formula:\n"
        "  range_mV = vdiv_mV * probe_factor * 10  (10 vertical divisions)\n"
        "  voltage_mV = (hw_offset - raw_sample) * range_mV / 255\n"
        "  where raw_sample is 0-255 (8-bit ADC)\n"
    ),
)

# ---------------------------------------------------------------------------
# Output format helper
# ---------------------------------------------------------------------------


def _dumps(obj, indent=None):
    """Serialize obj to TOON string if toon is available, else JSON."""
    if _HAS_TOON:
        return toon.encode(obj)
    return json.dumps(obj, indent=indent)


def _loads(s):
    """Deserialize from TOON or JSON string."""
    if _HAS_TOON:
        try:
            return toon.decode(s)
        except Exception:
            pass
    return json.loads(s)

# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------


def _run_cli(*args, timeout=30):
    """Run dsview-cli, return (stdout, stderr, returncode)."""
    if not CLI_BINARY.exists():
        msg = (f"dsview-cli not found at {CLI_BINARY}. "
               f"Build from DSView root: mkdir -p build && cd build && cmake .. && make")
        return "", msg, -1
    cmd = [str(CLI_BINARY)] + [str(a) for a in args]
    try:
        r = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout)
        return r.stdout, r.stderr, r.returncode
    except subprocess.TimeoutExpired:
        return "", f"dsview-cli timed out after {timeout}s", -1
    except Exception as exc:
        return "", str(exc), -1


def _parse_json(stdout, stderr, rc, context=""):
    text = stdout.strip()
    if not text:
        return {"error": stderr.strip() or f"{context}: no output (rc={rc})"}
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return {"error": f"invalid JSON: {text[:200]}"}


def _expand_channels(spec: str, max_ch: int = 16) -> list[int]:
    """
    Convert a channel spec string to a sorted list of channel indices.
      "all"     -> [0..max_ch-1]
      "0-7"     -> [0,1,2,3,4,5,6,7]
      "0,2,4,6" -> [0,2,4,6]
      "8"       -> [0..7]   (treated as a count)
    """
    spec = spec.strip().lower()
    if spec == "all":
        return list(range(max_ch))
    if "-" in spec and "," not in spec:
        lo, hi = spec.split("-", 1)
        return list(range(int(lo), int(hi) + 1))
    if "," in spec:
        return sorted(int(x) for x in spec.split(",") if x.strip().isdigit())
    # plain integer -> treat as count
    n = int(spec)
    return list(range(min(n, max_ch)))


def _parse_si(s: str) -> float:
    """Parse an SI-suffixed string to a numeric value.

    Supports both rate-style ('1M' = 1e6) and duration-style
    ('500ms' = 0.5, '100us' = 1e-4) suffixes.

    Examples:
        '1M'    -> 1_000_000.0
        '10k'   -> 10_000.0
        '500M'  -> 500_000_000.0
        '1G'    -> 1_000_000_000.0
        '200ms' -> 0.2
        '100us' -> 0.0001
        '50ns'  -> 5e-8
        '1s'    -> 1.0
        '0.5'   -> 0.5
    """
    s = s.strip()
    # Duration suffixes (must check multi-char before single-char)
    _DURATION_SUFFIXES = [
        ("ms", 1e-3),
        ("us", 1e-6),
        ("ns", 1e-9),
        ("ps", 1e-12),
        ("s", 1.0),
    ]
    s_lower = s.lower()
    for suffix, mult in _DURATION_SUFFIXES:
        if s_lower.endswith(suffix):
            return float(s[:len(s) - len(suffix)]) * mult

    # Rate-style SI suffixes (single char)
    _SI_SUFFIXES = {
        "k": 1e3, "K": 1e3,
        "m": 1e6, "M": 1e6,
        "g": 1e9, "G": 1e9,
    }
    if s and s[-1] in _SI_SUFFIXES:
        return float(s[:-1]) * _SI_SUFFIXES[s[-1]]

    return float(s)


def _load_meta(bin_path: str) -> dict:
    """Load the .meta.json sidecar if it exists."""
    meta_path = bin_path + ".meta.json"
    try:
        with open(meta_path) as f:
            return json.load(f)
    except Exception:
        return {}


# ---------------------------------------------------------------------------
# Export format helpers
# ---------------------------------------------------------------------------

def _fmt_samplerate(hz: int) -> str:
    """Format samplerate as human-readable string for SR metadata."""
    for unit, divisor in (("GHz", 1_000_000_000), ("MHz", 1_000_000),
                          ("kHz", 1_000)):
        if hz >= divisor and hz % divisor == 0:
            return f"{hz // divisor} {unit}"
    return f"{hz} Hz"


def _fmt_rate(hz: float) -> str:
    """Format a frequency (Hz) as a compact human-readable string."""
    abs_hz = abs(hz)
    if abs_hz >= 1_000_000_000:
        return "%.3g GHz" % (hz / 1e9)
    if abs_hz >= 1_000_000:
        return "%.3g MHz" % (hz / 1e6)
    if abs_hz >= 1_000:
        return "%.3g kHz" % (hz / 1e3)
    return "%.3g Hz" % hz


def _write_sigrok_binary(bin_path: str, out_path: str, meta: dict) -> None:
    """Write sigrok binary (our .bin minus 12-byte header)."""
    with open(bin_path, "rb") as f:
        f.seek(12)
        data = f.read()
    with open(out_path, "wb") as f:
        f.write(data)


def _write_sr(bin_path: str, out_path: str, meta: dict) -> None:
    """Write sigrok session ZIP (.sr) from a .bin capture.

    For logic captures: standard sigrok logic format with probe mapping.
    For DSO/analog captures: sigrok analog format with per-channel data files.
    """
    mode = meta.get("mode", "logic")
    if mode in ("dso", "analog"):
        _write_sr_dso(bin_path, out_path, meta)
    else:
        _write_sr_logic(bin_path, out_path, meta)


def _write_sr_logic(bin_path: str, out_path: str, meta: dict) -> None:
    """Write sigrok session ZIP for logic captures."""
    ch_map = meta.get("channel_map", [])
    samplerate = meta.get("samplerate", 1_000_000)
    unitsize = meta.get("unitsize", 1)
    n_probes = len(ch_map)

    lines = [
        "[global]",
        "sigrok version=0.5.2",
        "",
        "[device 1]",
        "capturefile=logic-1",
        "total probes=%d" % n_probes,
        "samplerate=%s" % _fmt_samplerate(samplerate),
        "total analog=0",
    ]
    for i, entry in enumerate(ch_map, start=1):
        name = entry.get("name") or ("CH%d" % entry.get("phys", i - 1))
        lines.append("probe%d=%s" % (i, name))
    lines.append("unitsize=%d" % unitsize)
    lines.append("")
    metadata_bytes = "\n".join(lines).encode()

    with open(bin_path, "rb") as f:
        f.seek(12)
        logic_data = f.read()

    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("version", "2")
        zf.writestr("metadata", metadata_bytes)
        zf.writestr("logic-1-1", logic_data)


def _write_sr_dso(bin_path: str, out_path: str, meta: dict) -> None:
    """Write sigrok session ZIP for DSO/analog captures.

    Sigrok analog format: each channel gets its own data file
    (analog-1-N-1) with 32-bit float samples in native byte order.
    """
    ch_map = meta.get("channel_map", [])
    samplerate = meta.get("samplerate", 1_000_000)
    n_ch = len(ch_map)

    with open(bin_path, "rb") as f:
        f.seek(12)
        raw = f.read()

    n_samples = len(raw) // n_ch if n_ch > 0 else 0

    lines = [
        "[global]",
        "sigrok version=0.5.2",
        "",
        "[device 1]",
        "total probes=0",
        "samplerate=%s" % _fmt_samplerate(samplerate),
        "total analog=%d" % n_ch,
    ]
    for i, entry in enumerate(ch_map, start=1):
        name = entry.get("name") or ("CH%d" % entry.get("phys", i - 1))
        lines.append("analog%d=%s" % (i, name))
    lines.append("")
    metadata_bytes = "\n".join(lines).encode()

    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("version", "2")
        zf.writestr("metadata", metadata_bytes)

        # Write per-channel analog data as 32-bit floats (voltage in V)
        for seq, entry in enumerate(ch_map):
            vdiv = entry.get("vdiv_mV", 1000)
            pfact = entry.get("probe_factor", 1)
            hw_off = entry.get("hw_offset", 128)

            floats = []
            for i in range(n_samples):
                sample = raw[i * n_ch + seq]
                v_mV = _raw_to_voltage_mV(sample, vdiv, pfact, hw_off)
                floats.append(v_mV / 1000.0)  # sigrok uses volts

            float_data = struct.pack("<%df" % len(floats), *floats)
            zf.writestr("analog-1-%d-1" % (seq + 1), float_data)


_VCD_IDS = [chr(c) for c in range(33, 127) if c != 36]  # skip '$'


def _write_vcd(bin_path: str, out_path: str, meta: dict) -> None:
    """Write VCD file from a .bin capture.

    For logic captures: standard VCD with 1-bit wire signals.
    For DSO captures: VCD is inherently digital, so analog signals are
    thresholded at the midpoint (hw_offset) to produce 0/1 waveforms.
    Use CSV export for actual voltage values.
    """
    import datetime
    ch_map = meta.get("channel_map", [])
    samplerate = meta.get("samplerate", 1_000_000)
    unitsize = meta.get("unitsize", 1)
    mode = meta.get("mode", "logic")
    is_dso = mode in ("dso", "analog")

    # Compute a VCD timescale so that the inter-sample period is always
    # an integer >= 1.  Walk from the largest unit down to picoseconds
    # and pick the first where period is a whole number >= 1.
    _VCD_UNITS = [
        ("s", 1),
        ("ms", 1_000),
        ("us", 1_000_000),
        ("ns", 1_000_000_000),
        ("ps", 1_000_000_000_000),
    ]
    ts_unit = "ps"
    period = 1
    if samplerate > 0:
        for unit_name, divisor in _VCD_UNITS:
            p = divisor / samplerate
            if p >= 1.0 and p == int(p):
                ts_unit = unit_name
                period = int(p)
                break
        else:
            ts_unit = "ps"
            period = max(1, round(1_000_000_000_000 / samplerate))

    with open(bin_path, "rb") as f:
        f.seek(12)
        raw = f.read()

    if is_dso:
        n_ch = len(ch_map)
        n_samples = len(raw) // n_ch if n_ch > 0 else 0

        # Build entries with threshold info
        entries = []
        for i, entry in enumerate(ch_map):
            name = entry.get("name") or ("CH%d" % entry.get("phys", i))
            ident = _VCD_IDS[i % len(_VCD_IDS)]
            hw_off = entry.get("hw_offset", 128)
            entries.append((ident, i, name, hw_off))

        with open(out_path, "w") as f:
            now = datetime.datetime.now().strftime("%a %b %d %H:%M:%S %Y")
            f.write("$date %s $end\n" % now)
            f.write("$version dsview-cli (DSO thresholded) $end\n")
            f.write("$comment analog thresholded at hw_offset $end\n")
            f.write("$timescale 1 %s $end\n" % ts_unit)
            f.write("$scope module dso $end\n")
            for ident, _, name, _ in entries:
                f.write("$var wire 1 %s %s $end\n" % (ident, name))
            f.write("$upscope $end\n")
            f.write("$enddefinitions $end\n")

            prev = {}
            for s in range(n_samples):
                changes = []
                for ident, seq, _, hw_off in entries:
                    sample = raw[s * n_ch + seq]
                    b = 1 if sample >= hw_off else 0
                    if prev.get(ident) != b:
                        changes.append("%d%s" % (b, ident))
                        prev[ident] = b
                if changes:
                    f.write("#%d %s\n" % (s * period, " ".join(changes)))
            f.write("#%d\n" % (n_samples * period))
    else:
        # Logic mode
        n_samples = len(raw) // unitsize

        entries = []
        for i, entry in enumerate(ch_map):
            bit = entry.get("phys", entry.get("seq", i))
            name = entry.get("name") or ("CH%d" % bit)
            ident = _VCD_IDS[i % len(_VCD_IDS)]
            entries.append((ident, bit, name))

        with open(out_path, "w") as f:
            now = datetime.datetime.now().strftime("%a %b %d %H:%M:%S %Y")
            f.write("$date %s $end\n" % now)
            f.write("$version dsview-cli $end\n")
            f.write("$timescale 1 %s $end\n" % ts_unit)
            f.write("$scope module logic $end\n")
            for ident, _, name in entries:
                f.write("$var wire 1 %s %s $end\n" % (ident, name))
            f.write("$upscope $end\n")
            f.write("$enddefinitions $end\n")

            prev = {}
            for s in range(n_samples):
                off = s * unitsize
                val = raw[off] if unitsize == 1 else struct.unpack_from("<H", raw, off)[
                    0]
                changes = []
                for ident, bit, _ in entries:
                    b = (val >> bit) & 1
                    if prev.get(ident) != b:
                        changes.append("%d%s" % (b, ident))
                        prev[ident] = b
                if changes:
                    f.write("#%d %s\n" % (s * period, " ".join(changes)))
            f.write("#%d\n" % (n_samples * period))


def _write_csv(bin_path: str, out_path: str, meta: dict) -> None:
    """Write CSV from a .bin capture.

    For logic captures: sigrok-compatible CSV with 0/1 bit values.
    For DSO/analog captures: time + voltage columns in mV.
    """
    ch_map = meta.get("channel_map", [])
    samplerate = meta.get("samplerate", 1_000_000)
    unitsize = meta.get("unitsize", 1)
    mode = meta.get("mode", "logic")

    with open(bin_path, "rb") as f:
        f.seek(12)
        raw = f.read()

    if mode in ("dso", "analog"):
        _write_csv_dso(raw, out_path, meta, ch_map, samplerate)
    else:
        _write_csv_logic(raw, out_path, ch_map, samplerate, unitsize)


def _write_csv_logic(raw: bytes, out_path: str, ch_map: list,
                     samplerate: int, unitsize: int) -> None:
    """Write sigrok-compatible logic CSV with 0/1 values."""
    names = []
    bits = []
    for i, entry in enumerate(ch_map):
        bit = entry.get("phys", entry.get("seq", i))
        name = entry.get("name") or ("CH%d" % bit)
        names.append(name)
        bits.append(bit)

    n_samples = len(raw) // unitsize

    with open(out_path, "w") as f:
        f.write("; Channels (%d/%d): %s\n"
                % (len(names), len(names), ", ".join(names)))
        f.write("; Samplerate: %d Hz\n" % samplerate)
        f.write(",".join(names) + "\n")
        for s in range(n_samples):
            off = s * unitsize
            val = raw[off] if unitsize == 1 else struct.unpack_from("<H", raw, off)[
                0]
            f.write(",".join(str((val >> b) & 1) for b in bits) + "\n")


def _write_csv_dso(raw: bytes, out_path: str, meta: dict, ch_map: list,
                   samplerate: int) -> None:
    """Write DSO CSV with time and voltage columns in mV."""
    n_ch = len(ch_map)
    if n_ch == 0:
        return
    n_samples = len(raw) // n_ch

    # Build channel info
    ch_info = []
    for seq, entry in enumerate(ch_map):
        label = entry.get("name") or ("CH%d" % entry.get("phys", seq))
        ch_info.append({
            "label": label,
            "vdiv": entry.get("vdiv_mV", 1000),
            "pfact": entry.get("probe_factor", 1),
            "hw_off": entry.get("hw_offset", 128),
        })

    names = [ci["label"] + "_mV" for ci in ch_info]
    time_step = 1.0 / samplerate if samplerate > 0 else 0.0

    with open(out_path, "w") as f:
        f.write("; DSO capture -- voltages in millivolts\n")
        f.write("; Samplerate: %d Hz\n" % samplerate)
        for ci in ch_info:
            f.write("; %s: vdiv=%d mV, probe_factor=%d, coupling=%s\n"
                    % (ci["label"], ci["vdiv"], ci["pfact"], "DC"))
        f.write("Time_s," + ",".join(names) + "\n")
        for s in range(n_samples):
            t = s * time_step
            vals = []
            for seq, ci in enumerate(ch_info):
                sample = raw[s * n_ch + seq]
                v = _raw_to_voltage_mV(sample, ci["vdiv"], ci["pfact"],
                                       ci["hw_off"])
                vals.append("%.2f" % v)
            f.write("%.9f,%s\n" % (t, ",".join(vals)))


_VALID_FORMATS = {"bin", "sigrok-binary", "vcd", "csv", "sr"}


# ---------------------------------------------------------------------------
# MCP tools
# ---------------------------------------------------------------------------

@mcp.tool()
def scan_devices() -> str:
    """Scan for connected DreamSourceLab USB devices.

    Returns a list of all detected devices with index, handle, and name.
    Multiple devices can be connected simultaneously; use the index value
    in other tools (device_info, capture_logic) to address a specific unit.

    Call this first to discover what is plugged in and how many devices exist.
    """
    stdout, stderr, rc = _run_cli("scan", timeout=10)
    data = _parse_json(stdout, stderr, rc, "scan")
    if isinstance(data, list):
        result = {"count": len(data), "devices": data}
        result["note"] = (
            "Use device_index=N in other tools to address a specific device. "
            "Call device_info(N) to learn the instrument's capabilities."
        )
        return _dumps(result)
    return _dumps(data)


@mcp.tool()
def device_info(device_index: int = 0) -> str:
    """Get full capabilities of a connected DreamSourceLab instrument.

    Returns:
    - name: exact device model (e.g. 'DSLogic U3Pro16' or 'DSCope U3P100')
    - mode_name: device mode -- 'LOGIC', 'DSO', or 'ANALOG'
    - dsview_version: DSView software version (e.g. '1.3.2')
    - libsigrok4dsl_version: library version string
    - channels: total physical channel count
    - channel_range: valid channel indices [min, max]
    - samplerate: current samplerate in Hz
    - channel_modes: list of available channel configurations, each showing
      how many channels can be used at what maximum samplerate.
    - trigger_types: supported trigger conditions
    - limit_samples: current sample count limit

    For LOGIC mode:
    - vth: current voltage threshold in volts (Pro devices)

    For DSO/ANALOG mode:
    - analog_channels: per-channel info with vdiv_mV, probe_factor, coupling,
      hw_offset, bits
    - vdiv_options: available voltage-per-division settings [10..2000 mV]
    - coupling_options: ['DC', 'AC']
    - probe_factor_options: [1, 2, 10, 20]

    IMPORTANT: check mode_name to determine which capture tool to use:
    - mode_name='LOGIC' -> use capture_logic()
    - mode_name='DSO' or 'ANALOG' -> use capture_dso()

    Args:
        device_index: Device index from scan_devices (default 0).
                      Use scan_devices first if multiple units are connected.
    """
    stdout, stderr, rc = _run_cli("info", "-d", device_index, timeout=15)
    data = _parse_json(stdout, stderr, rc, "info")
    if isinstance(data, dict) and "channel_modes" in data:
        # Add human-readable summary of the channels-vs-samplerate tradeoff
        modes = data["channel_modes"]
        if modes:
            data["channel_modes_summary"] = (
                "More active channels = lower max samplerate. "
                f"Available configurations: {len(modes)}"
            )

    if not isinstance(data, dict):
        return _dumps(data)

    mode_name = data.get("mode_name", "LOGIC")

    # Add mode-specific guidance
    if mode_name in ("DSO", "ANALOG"):
        data["usage_hint"] = (
            "This is an oscilloscope (mode=%s). Use capture_dso() to "
            "record analog waveforms, then decode_analog() to view "
            "voltage values. Set vdiv to match expected signal "
            "amplitude. Full-scale range = vdiv * probe_factor * 10 "
            "divisions." % mode_name
        )
        # Add voltage-conversion formula hint
        data["voltage_formula"] = (
            "voltage_mV = (hw_offset - raw_sample) * vdiv_mV * "
            "probe_factor * 10 / 255"
        )
    else:
        data["usage_hint"] = (
            "This is a logic analyzer (mode=LOGIC). Use capture_logic() "
            "to record digital signals. Set voltage_threshold to match "
            "your target logic levels."
        )

    # Compute a capture-budget table so Claude can see the
    # samplerate-vs-duration tradeoff for this device.
    if data.get("limit_samples"):
        limit = data["limit_samples"]
        _BUDGET_RATES = [
            100_000, 500_000,
            1_000_000, 4_000_000, 10_000_000, 25_000_000,
            50_000_000, 100_000_000, 250_000_000, 500_000_000,
            1_000_000_000,
        ]
        budget = []
        for rate in _BUDGET_RATES:
            dur_s = limit / rate
            if dur_s < 0.001:
                dur_str = "%.1f us" % (dur_s * 1_000_000)
            elif dur_s < 1.0:
                dur_str = "%.1f ms" % (dur_s * 1_000)
            else:
                dur_str = "%.1f s" % dur_s
            budget.append({
                "samplerate": _fmt_samplerate(rate),
                "max_duration": dur_str,
            })
        data["capture_budget"] = budget

        capture_tool = "capture_dso" if mode_name in (
            "DSO", "ANALOG") else "capture_logic"
        data["capture_budget_note"] = (
            "Max capture duration at each samplerate given the device "
            "sample memory (%d samples). Use the lowest samplerate that "
            "meets the Nyquist requirement (>= 4x signal frequency) to "
            "maximize capture duration. Use 'duration' parameter in "
            "%s() to specify capture time directly." % (limit, capture_tool)
        )

    return _dumps(data)


@mcp.tool()
def capture_logic(
    device_index: int = 0,
    samplerate: str = "1M",
    num_samples: str = "1M",
    duration: str = "",
    channels: str = "all",
    channel_names: str = "",
    trigger_channel: int = -1,
    trigger_type: str = "none",
    trigger_pos: int = 50,
    out_file: str = "",
    out_format: str = "bin",
    voltage_threshold: float = -1.0,
    timeout: int = 180,
) -> str:
    """Capture logic-analyzer data from a DreamSourceLab device.

    IMPORTANT: check device_info first to see which channel_mode is active
    and what samplerate is achievable for your channel count.  Fewer channels
    allow higher samplerates (e.g. 6 channels -> 500 MHz on DSLogic U3Pro16).

    Args:
        device_index:    Device index from scan_devices (default 0).
                         Use different indices for multiple connected devices.
        samplerate:      Sample rate with SI suffix: 100k, 1M, 10M, 100M, 500M.
        num_samples:     Samples to capture: 10k, 100k, 1M, 10M.
                         Ignored when duration is provided.
        duration:        Capture duration with time suffix: '100ms', '1s', '500us'.
                         When provided, num_samples is computed as samplerate * duration.
                         Supported suffixes: s, ms, us, ns.
                         Examples: '1s' = 1 second, '500ms' = 500 milliseconds,
                         '100us' = 100 microseconds.
                         Leave empty to use num_samples instead.
        channels:        Which physical channels to record.
                         Formats: "all" | "0-7" | "0,2,4,6" | "8" (count).
        channel_names:   Comma-separated signal names in the SAME ORDER as
                         the enabled channels. e.g. "SDA,SCL" for channels="0,1"
                         or "D0,D1,D2,D3,CLK,CS,MOSI,MISO" for channels="0-7".
                         Leave empty to use default CH0..CHn labels.
        trigger_channel: Physical channel index to trigger on (-1 = free-run).
                         Must be one of the enabled channels.
        trigger_type:    Trigger condition: none | rising | falling | high | low.
        trigger_pos:     Percentage of samples BEFORE the trigger point (0-100).
                         50 = trigger at centre of capture window.
        out_file:        Full path for the .bin output. Auto-generated if empty.
                         A .meta.json sidecar is always written alongside it.
        out_format:      Output format for export. Default 'bin' (native .bin + .meta.json).
                         'sigrok-binary' - raw bytes without header (use with sigrok-mcp-server
                                           -I binary:numchannels=N:samplerate=M)
                         'vcd'           - Value Change Dump (IEEE standard, channel names preserved)
                         'csv'           - sigrok-compatible CSV (use with -I csv:samplerate=M)
                         'sr'            - sigrok session ZIP (.sr), native sigrok-mcp-server format
                         Native .bin + .meta.json are always written alongside the export.
        voltage_threshold: Input voltage threshold in volts (0.0-5.0).
                         Controls the logic-level switching point for Pro devices
                         that support variable thresholds (e.g. DSLogic U3Pro16).
                         Default -1.0 = use device default (typically 1.0V).
                         Common values: 0.8 (LVCMOS), 1.0, 1.2, 1.5, 1.8, 2.5, 3.3.
        timeout:         Maximum seconds to wait for dsview-cli to complete (default 180).
                         Increase for long captures (e.g. waiting for an infrequent trigger).
                         The process is killed and an error returned if the timeout expires.

    Returns:
        Capture status, channel map, trigger settings, and a 32-sample waveform
        preview.
    """
    # Validate export format
    if out_format not in _VALID_FORMATS:
        return _dumps({"error": f"invalid out_format {out_format!r}. "
                                f"Valid: {sorted(_VALID_FORMATS)}"})

    # Validate voltage threshold
    if voltage_threshold >= 0.0 and voltage_threshold > 5.0:
        return _dumps({
            "error": f"voltage_threshold {voltage_threshold} exceeds 5.0V maximum",
            "hint": "Common values: 0.8 (LVCMOS), 1.0 (default), 1.5 (3.3V), 2.5 (5V TTL)",
        })

    # Convert duration to num_samples if provided
    if duration.strip():
        try:
            rate_hz = _parse_si(samplerate)
            dur_s = _parse_si(duration)
        except (ValueError, TypeError) as exc:
            return _dumps(
                {"error": f"cannot parse samplerate/duration: {exc}"})
        if dur_s <= 0:
            return _dumps(
                {"error": f"duration must be positive, got {duration!r}"})
        if rate_hz <= 0:
            return _dumps(
                {"error": f"samplerate must be positive, got {samplerate!r}"})
        computed = round(rate_hz * dur_s)
        if computed < 1:
            return _dumps({
                "error": f"duration {duration!r} at samplerate {samplerate!r} "
                         f"yields < 1 sample ({rate_hz * dur_s:.4g})",
                "hint": "increase duration or samplerate",
            })
        num_samples = str(computed)

    # Resolve output path
    if not out_file:
        fd, base_path = tempfile.mkstemp(prefix="dsview_")
        os.close(fd)
        out_file = base_path + ".bin"
    elif out_file.endswith(".bin"):
        base_path = out_file[:-4]
    else:
        base_path = out_file
        out_file = base_path + ".bin"

    # Expand channel spec to a sorted list of indices
    try:
        ch_list = _expand_channels(channels)
    except (ValueError, TypeError):
        return _dumps({"error": f"invalid channels spec: {channels!r}"})

    if not ch_list:
        return _dumps({"error": "no channels selected"})

    # Validate trigger channel is in the enabled set (if specified)
    if trigger_channel >= 0 and trigger_channel not in ch_list:
        return _dumps({
            "error": f"trigger_channel {trigger_channel} is not in enabled channels {ch_list}",
            "hint": "trigger_channel must be one of the enabled channel indices",
        })

    if trigger_type not in ("none", "rising", "falling", "high", "low"):
        return _dumps({"error": f"invalid trigger_type: {trigger_type!r}. "
                                "Use: none|rising|falling|high|low"})

    if not 0 <= trigger_pos <= 100:
        return _dumps(
            {"error": f"trigger_pos must be 0-100, got {trigger_pos}"})

    # Build CLI arguments
    enable_str = ",".join(str(c) for c in ch_list)

    cli_args = [
        "capture",
        "-d", device_index,
        "-s", samplerate,
        "-n", num_samples,
        "-c", enable_str,
        "-t", trigger_channel,
        "-T", trigger_type,
        "-p", trigger_pos,
        "-o", out_file,
    ]

    if channel_names.strip():
        cli_args += ["-N", channel_names.strip()]

    if voltage_threshold >= 0.0:
        cli_args += ["-V", str(voltage_threshold)]

    stdout, stderr, rc = _run_cli(*cli_args, timeout=timeout)
    result = _parse_json(stdout, stderr, rc, "capture")

    if not isinstance(result, dict) or not result.get("success"):
        return _dumps(result)

    # Include computed duration info when duration was specified
    if duration.strip() and result.get("success"):
        result["requested_duration"] = duration.strip()
        result["computed_num_samples"] = num_samples

    # Enrich with file size and 32-sample preview
    cap_path = Path(out_file)
    if cap_path.exists():
        result["file_size_bytes"] = cap_path.stat().st_size
        unitsize = result.get("unitsize", 1)
        ch_map = result.get("channel_map", [])

        try:
            with open(out_file, "rb") as f:
                f.seek(12)  # skip 12-byte header
                raw = f.read(unitsize * 32)

            n_got = len(raw) // unitsize
            preview = []
            for i in range(n_got):
                off = i * unitsize
                val = raw[off] if unitsize == 1 else struct.unpack_from("<H", raw, off)[
                    0]
                row = {}
                for entry in ch_map:
                    bit = entry.get("phys", entry.get("seq", 0))
                    label = entry.get(
                        "name") or f"CH{entry.get('phys', entry.get('seq'))}"
                    row[label] = (val >> bit) & 1
                preview.append(row)

            result["sample_preview"] = {
                "note": f"First {n_got} samples (columns = signal names)",
                "samples": preview,
            }
        except Exception as exc:
            result["preview_error"] = str(exc)

    # Export to requested format
    if out_format != "bin" and isinstance(
            result, dict) and result.get("success"):
        meta = _load_meta(out_file)
        ext_map = {
            "sigrok-binary": ".sigrok.bin",
            "vcd": ".vcd",
            "csv": ".csv",
            "sr": ".sr",
        }
        export_path = base_path + ext_map[out_format]
        try:
            if out_format == "sigrok-binary":
                _write_sigrok_binary(out_file, export_path, meta)
                n_ch = len(meta.get("channel_map", []))
                sr = meta.get("samplerate", 0)
                result["export_file"] = export_path
                result["sigrok_format_string"] = f"binary:numchannels={n_ch}:samplerate={sr}"
            elif out_format == "vcd":
                _write_vcd(out_file, export_path, meta)
                result["export_file"] = export_path
            elif out_format == "csv":
                _write_csv(out_file, export_path, meta)
                sr = meta.get("samplerate", 0)
                result["export_file"] = export_path
                result["sigrok_format_string"] = f"csv:samplerate={sr}"
            elif out_format == "sr":
                _write_sr(out_file, export_path, meta)
                result["export_file"] = export_path
                result["sigrok_format_string"] = "srzip"
        except Exception as exc:
            result["export_error"] = str(exc)

    # Add protocol-decode hint when export format supports sigrok decoding
    if out_format in ("vcd", "sr", "csv",
                      "sigrok-binary") and result.get("success"):
        ch_map = result.get("channel_map", [])
        ch_names = [e.get("name", "") for e in ch_map if e.get("name")]
        hint_parts = [
            "Use sigrok decode_protocol tool for protocol analysis.",
        ]
        # Build example decoder strings based on channel names
        name_set = {n.upper() for n in ch_names}
        if {"SDA", "SCL"} <= name_set:
            hint_parts.append(
                "I2C detected: decode_protocol(input_file=<export_file>, "
                "protocol_decoders='i2c:scl=SCL:sda=SDA')"
            )
        if {"MOSI", "MISO", "CLK", "CS"} <= name_set or {
                "MOSI", "SCK", "CS"} <= name_set:
            hint_parts.append(
                "SPI detected: decode_protocol(input_file=<export_file>, "
                "protocol_decoders='spi:clk=CLK:mosi=MOSI:miso=MISO:cs=CS')"
            )
        if {"TX"} <= name_set or {"RX"} <= name_set:
            hint_parts.append(
                "UART detected: decode_protocol(input_file=<export_file>, "
                "protocol_decoders='uart:rx=RX:baudrate=115200') "
                "-- adjust baudrate as needed"
            )
        result["decode_hint"] = " ".join(hint_parts)

    return _dumps(result)


# ---------------------------------------------------------------------------
# DSO voltage helpers
# ---------------------------------------------------------------------------

def _raw_to_voltage_mV(raw: int, vdiv_mV: int, probe_factor: int,
                       hw_offset: int) -> float:
    """Convert raw 8-bit ADC sample to millivolts.

    Formula: voltage_mV = (hw_offset - raw) * vdiv * probe_factor * 10 / 255
    DS_CONF_DSO_VDIVS = 10 (number of vertical divisions).
    """
    range_mV = vdiv_mV * probe_factor * 10
    return (hw_offset - raw) * range_mV / 255.0


def _dso_preview(out_file: str, ch_map: list, max_samples: int = 32) -> list:
    """Read a few DSO samples and convert to voltage for preview."""
    n_ch = len(ch_map)
    if n_ch == 0:
        return []
    try:
        with open(out_file, "rb") as f:
            f.seek(12)  # skip header
            raw = f.read(n_ch * max_samples)
    except Exception:
        return []

    n_got = len(raw) // n_ch
    preview = []
    for i in range(n_got):
        row = {}
        for seq, entry in enumerate(ch_map):
            label = entry.get("name") or ("CH%d" % entry.get("phys", seq))
            sample = raw[i * n_ch + seq]
            vdiv = entry.get("vdiv_mV", 1000)
            pfact = entry.get("probe_factor", 1)
            hw_off = entry.get("hw_offset", 128)
            row[label] = round(
                _raw_to_voltage_mV(sample, vdiv, pfact, hw_off), 2
            )
        preview.append(row)
    return preview


@mcp.tool()
def capture_dso(
    device_index: int = 0,
    samplerate: str = "1M",
    num_samples: str = "10k",
    duration: str = "",
    channels: str = "all",
    channel_names: str = "",
    vdiv: str = "",
    coupling: str = "",
    probe_factor: str = "",
    trigger_channel: int = -1,
    trigger_type: str = "none",
    trigger_pos: int = 50,
    out_file: str = "",
    out_format: str = "bin",
    timeout: int = 180,
) -> str:
    """Capture analog waveform data from a DreamSourceLab oscilloscope.

    Use this for DScope devices (mode_name='DSO' or 'ANALOG' in device_info).
    For logic analyzers (mode_name='LOGIC'), use capture_logic() instead.

    IMPORTANT: call device_info() first to confirm mode_name is 'DSO' or
    'ANALOG' and to see the current analog_channels settings.

    Args:
        device_index:    Device index from scan_devices (default 0).
        samplerate:      Sample rate with SI suffix: 100k, 1M, 10M, 100M.
        num_samples:     Samples to capture: 10k, 100k, 1M, 10M.
                         Ignored when duration is provided.
        duration:        Capture duration with time suffix: '100ms', '1s', '500us'.
                         When provided, num_samples is computed automatically.
        channels:        Which analog channels to record.
                         "all" = both channels, "0" = CH0 only, "1" = CH1 only,
                         "0,1" = both channels.
        channel_names:   Comma-separated signal names in same order as channels.
                         e.g. "VOUT,GND" for channels="0,1".
        vdiv:            Voltage per division in mV. Sets the vertical scale.
                         Formats: "500" (both channels), "0:500,1:1000" (per-channel).
                         Full-scale range = vdiv * probe_factor * 10 divisions.
                         Options: 10, 20, 50, 100, 200, 500, 1000, 2000 mV.
        coupling:        Input coupling mode.
                         "DC" (both channels), "AC" (both), or "0:DC,1:AC".
                         DC passes the full signal; AC blocks the DC component.
        probe_factor:    Probe attenuation factor.
                         "1" (both channels), "10" (both), or "0:1,1:10".
                         Must match your physical probe: 1x, 2x, 10x, or 20x.
        trigger_channel: Analog channel to trigger on (-1 = free-run/auto).
                         Must be 0 or 1 (one of the enabled channels).
        trigger_type:    Trigger slope: none | rising | falling.
                         DSO trigger is edge-based (slope detection).
        trigger_pos:     Percentage of capture window before trigger (0-100).
                         50 = trigger at centre. 10 = mostly post-trigger data.
        out_file:        Full path for .bin output. Auto-generated if empty.
        out_format:      Output format: 'bin' (default), 'csv' (voltage values),
                         'sr' (sigrok session), 'vcd', 'sigrok-binary'.
        timeout:         Maximum seconds to wait for dsview-cli to complete (default 180).
                         Increase for long captures (e.g. waiting for an infrequent trigger).
                         The process is killed and an error returned if the timeout expires.

    Returns:
        Capture status, channel_map with analog metadata (vdiv, coupling,
        probe_factor, hw_offset), file path, and voltage preview of first
        32 samples.
    """
    # Validate export format
    if out_format not in _VALID_FORMATS:
        return _dumps({"error": "invalid out_format %r. Valid: %s"
                       % (out_format, sorted(_VALID_FORMATS))})

    # Convert duration to num_samples if provided
    if duration.strip():
        try:
            rate_hz = _parse_si(samplerate)
            dur_s = _parse_si(duration)
        except (ValueError, TypeError) as exc:
            return _dumps(
                {"error": "cannot parse samplerate/duration: %s" % exc})
        if dur_s <= 0:
            return _dumps(
                {"error": "duration must be positive, got %r" % duration})
        if rate_hz <= 0:
            return _dumps(
                {"error": "samplerate must be positive, got %r" % samplerate})
        computed = round(rate_hz * dur_s)
        if computed < 1:
            return _dumps({
                "error": "duration %r at samplerate %r yields < 1 sample"
                         % (duration, samplerate),
                "hint": "increase duration or samplerate",
            })
        num_samples = str(computed)

    # Resolve output path
    if not out_file:
        fd, base_path = tempfile.mkstemp(prefix="dsview_dso_")
        os.close(fd)
        out_file = base_path + ".bin"
    elif out_file.endswith(".bin"):
        base_path = out_file[:-4]
    else:
        base_path = out_file
        out_file = base_path + ".bin"

    # Expand channel spec for DSO.
    # _expand_channels treats a bare integer as a *count*, but for DSO a
    # bare "0" or "1" (or any valid index < max_ch) means a specific
    # channel index.  Normalise to comma-separated form so the parser
    # takes the list-of-indices branch instead.
    _dso_max_ch = 2                       # DSCope U3P100; adjust if needed
    _ch_spec = channels.strip()
    if _ch_spec.isdigit() and int(_ch_spec) < _dso_max_ch:
        _ch_spec = _ch_spec + ","         # "0," -> parsed as list [0]
    try:
        ch_list = _expand_channels(_ch_spec, max_ch=_dso_max_ch)
    except (ValueError, TypeError):
        return _dumps({"error": "invalid channels spec: %r" % channels})

    if not ch_list:
        return _dumps({"error": "no channels selected"})

    for c in ch_list:
        if c not in (0, 1):
            return _dumps({
                "error": "DSO channel index %d out of range. "
                         "DSCope has 2 channels: 0 and 1." % c,
            })

    # Validate trigger
    if trigger_channel >= 0 and trigger_channel not in ch_list:
        return _dumps({
            "error": "trigger_channel %d not in enabled channels %s"
                     % (trigger_channel, ch_list),
            "hint": "trigger_channel must be 0 or 1",
        })
    if trigger_type not in ("none", "rising", "falling"):
        return _dumps({
            "error": "invalid trigger_type: %r. DSO supports: none|rising|falling"
                     % trigger_type,
        })
    if not 0 <= trigger_pos <= 100:
        return _dumps(
            {"error": "trigger_pos must be 0-100, got %d" % trigger_pos})

    # Build CLI arguments
    enable_str = ",".join(str(c) for c in ch_list)

    cli_args = [
        "capture",
        "-d", device_index,
        "-s", samplerate,
        "-n", num_samples,
        "-c", enable_str,
        "-t", trigger_channel,
        "-T", trigger_type,
        "-p", trigger_pos,
        "-o", out_file,
    ]

    if channel_names.strip():
        cli_args += ["-N", channel_names.strip()]

    # DSO-specific per-channel options
    if vdiv.strip():
        for part in _parse_dso_option(vdiv, ch_list):
            cli_args += ["--vdiv", part]

    if coupling.strip():
        for part in _parse_dso_coupling_option(coupling, ch_list):
            cli_args += ["--coupling", part]

    if probe_factor.strip():
        for part in _parse_dso_option(probe_factor, ch_list):
            cli_args += ["--probe", part]

    stdout, stderr, rc = _run_cli(*cli_args, timeout=timeout)
    result = _parse_json(stdout, stderr, rc, "capture_dso")

    if not isinstance(result, dict) or not result.get("success"):
        return _dumps(result)

    # Include computed duration info
    if duration.strip() and result.get("success"):
        result["requested_duration"] = duration.strip()
        result["computed_num_samples"] = num_samples

    # Enrich with voltage preview
    cap_path = Path(out_file)
    if cap_path.exists():
        result["file_size_bytes"] = cap_path.stat().st_size
        ch_map = result.get("channel_map", [])
        preview = _dso_preview(out_file, ch_map, max_samples=32)
        if preview:
            result["sample_preview"] = {
                "note": "First %d samples (voltage in mV)" % len(preview),
                "samples": preview,
            }

    # Export to requested format
    if out_format != "bin" and isinstance(
            result, dict) and result.get("success"):
        meta = _load_meta(out_file)
        ext_map = {
            "sigrok-binary": ".sigrok.bin",
            "vcd": ".vcd",
            "csv": ".csv",
            "sr": ".sr",
        }
        export_path = base_path + ext_map[out_format]
        try:
            if out_format == "csv":
                _write_csv(out_file, export_path, meta)
                result["export_file"] = export_path
                result["csv_note"] = "CSV contains voltage values in mV"
            elif out_format == "sr":
                _write_sr(out_file, export_path, meta)
                result["export_file"] = export_path
            elif out_format == "vcd":
                _write_vcd(out_file, export_path, meta)
                result["export_file"] = export_path
            elif out_format == "sigrok-binary":
                _write_sigrok_binary(out_file, export_path, meta)
                result["export_file"] = export_path
        except Exception as exc:
            result["export_error"] = str(exc)

    # Add DSO-specific hints
    if result.get("success"):
        ch_map = result.get("channel_map", [])
        hints = []
        for entry in ch_map:
            vd = entry.get("vdiv_mV", 0)
            pf = entry.get("probe_factor", 1)
            if vd:
                full_range = vd * pf * 10
                label = entry.get("name") or ("CH%d" % entry.get("phys", 0))
                hints.append(
                    "%s: vdiv=%dmV x%d probe = +/-%.1fV full-scale"
                    % (label, vd, pf, full_range / 1000.0)
                )
        if hints:
            result["scale_info"] = hints
        result["decode_hint"] = (
            "Use decode_analog(file_path) to view voltage waveform. "
            "Use signal_summary(file_path) for Vpp/Vrms/frequency statistics."
        )

    return _dumps(result)


def _parse_dso_option(spec: str, ch_list: list) -> list:
    """Parse a DSO per-channel option like '500' or '0:500,1:1000'.

    Returns a list of 'CH:VALUE' strings suitable for CLI --vdiv / --probe.
    """
    spec = spec.strip()
    if ":" in spec:
        # Already in CH:VALUE format, possibly comma-separated
        return [p.strip() for p in spec.split(",") if p.strip()]
    # Plain value -- apply to all enabled channels
    return ["%d:%s" % (ch, spec) for ch in ch_list]


def _parse_dso_coupling_option(spec: str, ch_list: list) -> list:
    """Parse coupling spec: 'DC', 'AC', or '0:DC,1:AC'.

    Returns list of 'CH:MODE' strings for CLI --coupling.
    """
    spec = spec.strip()
    if ":" in spec:
        return [p.strip() for p in spec.split(",") if p.strip()]
    # Plain mode -- apply to all enabled channels
    return ["%d:%s" % (ch, spec) for ch in ch_list]


@mcp.tool()
def decode_analog(
    file_path: str,
    start_sample: int = 0,
    num_samples: int = 256,
) -> str:
    """Read and decode analog voltage samples from a DSO capture .bin file.

    Converts raw 8-bit ADC samples to millivolt values using the calibration
    data stored in the .meta.json sidecar (vdiv, probe_factor, hw_offset).

    For each channel returns:
    - Voltage waveform (mV) for the requested sample range
    - Statistics: min, max, Vpp, Vrms, DC offset (mean)
    - Approximate frequency (zero-crossing based)

    Args:
        file_path:    Path to the .bin file from capture_dso.
        start_sample: First sample to decode (0 = start of capture).
        num_samples:  Number of samples to return (max 1024).

    Returns:
        Per-channel voltage waveforms and statistics.
    """
    if not Path(file_path).exists():
        return _dumps({"error": "file not found: %s" % file_path})

    num_samples = min(num_samples, 1024)
    meta = _load_meta(file_path)

    if not meta:
        return _dumps(
            {"error": "no .meta.json sidecar found for %s" % file_path})

    mode = meta.get("mode", "logic")
    if mode not in ("dso", "analog"):
        return _dumps({
            "error": "file mode is '%s', not DSO/analog. "
                     "Use decode_capture() for logic captures." % mode,
        })

    samplerate = meta.get("samplerate", 1_000_000)
    ch_map = meta.get("channel_map", [])
    n_ch = len(ch_map)

    if n_ch == 0:
        return _dumps({"error": "no channels in metadata"})

    header_size = 12
    # DSO data: interleaved 8-bit, 1 byte per channel per sample
    byte_offset = header_size + start_sample * n_ch

    try:
        with open(file_path, "rb") as f:
            f.seek(byte_offset)
            raw = f.read(num_samples * n_ch)
    except Exception as exc:
        return _dumps({"error": str(exc)})

    n_got = len(raw) // n_ch
    if n_got == 0:
        return _dumps({"error": "no sample data at offset %d" % start_sample})

    sample_period_ns = int(1e9 / samplerate) if samplerate > 0 else 0

    # Build per-channel voltage arrays and statistics
    channels_result = {}
    for seq, entry in enumerate(ch_map):
        label = entry.get("name") or ("CH%d" % entry.get("phys", seq))
        vdiv = entry.get("vdiv_mV", 1000)
        pfact = entry.get("probe_factor", 1)
        hw_off = entry.get("hw_offset", 128)

        voltages = []
        for i in range(n_got):
            sample = raw[i * n_ch + seq]
            v = _raw_to_voltage_mV(sample, vdiv, pfact, hw_off)
            voltages.append(round(v, 2))

        # Statistics
        v_min = min(voltages)
        v_max = max(voltages)
        v_pp = v_max - v_min
        v_mean = sum(voltages) / len(voltages)
        v_rms = math.sqrt(sum(v * v for v in voltages) / len(voltages))

        # Frequency estimation via zero-crossing (relative to mean)
        crossings = 0
        for i in range(1, len(voltages)):
            if ((voltages[i - 1] - v_mean) * (voltages[i] - v_mean)) < 0:
                crossings += 1

        freq = None
        if crossings >= 2 and samplerate > 0:
            freq = round((crossings / 2.0) / (n_got / samplerate), 1)

        ch_result = {
            "vdiv_mV": vdiv,
            "probe_factor": pfact,
            "coupling": entry.get("coupling", "DC"),
            "hw_offset": hw_off,
            "full_scale_mV": vdiv * pfact * 10,
            "stats": {
                "min_mV": round(v_min, 2),
                "max_mV": round(v_max, 2),
                "vpp_mV": round(v_pp, 2),
                "vrms_mV": round(v_rms, 2),
                "dc_offset_mV": round(v_mean, 2),
            },
            "voltages_mV": voltages,
        }
        if freq is not None:
            ch_result["stats"]["approx_freq_hz"] = freq
            if samplerate > 0 and freq > 0:
                ch_result["stats"]["min_samplerate_hz"] = round(freq * 4)
                ch_result["stats"]["rec_samplerate_hz"] = round(freq * 10)

        channels_result[label] = ch_result

    return _dumps({
        "file": file_path,
        "mode": mode,
        "samplerate": samplerate,
        "start_sample": start_sample,
        "samples_decoded": n_got,
        "sample_period_ns": sample_period_ns,
        "trigger": meta.get("trigger", {}),
        "channels": channels_result,
    })


@mcp.tool()
def decode_capture(
    file_path: str,
    start_sample: int = 0,
    num_samples: int = 256,
) -> str:
    """Read and decode samples from a previously captured .bin file.

    Automatically reads channel names and sample rate from the companion
    .meta.json sidecar written by capture_logic or capture_dso.

    For logic captures: returns per-signal digital waveform and edge list.
    For DSO captures: automatically delegates to decode_analog() for
    voltage waveforms.

    Args:
        file_path:    Path to the .bin file from capture_logic or capture_dso.
        start_sample: First sample to decode (0 = from trigger point).
        num_samples:  Samples to return (max 1024).

    Returns:
        Per-signal waveform data and an edge (transition) list.
    """
    if not Path(file_path).exists():
        return _dumps({"error": "file not found: %s" % file_path})

    num_samples = min(num_samples, 1024)
    meta = _load_meta(file_path)

    # Auto-redirect DSO captures to decode_analog
    if meta.get("mode") in ("dso", "analog"):
        return decode_analog(file_path, start_sample, num_samples)

    samplerate = meta.get("samplerate", 1_000_000)
    ch_map = meta.get("channel_map", [])
    unitsize = meta.get("unitsize", 2)
    header_size = 12

    # If no meta, read header from file
    if not meta:
        try:
            with open(file_path, "rb") as f:
                hdr = f.read(header_size)
            if len(hdr) == header_size:
                samplerate, n_ch_hdr = struct.unpack("<QI", hdr)
                ch_map = [{"seq": i, "phys": i, "name": "CH%d" % i}
                          for i in range(n_ch_hdr)]
                unitsize = 2 if n_ch_hdr > 8 else 1
        except Exception:
            pass

    if not ch_map:
        return _dumps(
            {"error": "cannot determine channel map; no .meta.json sidecar"})

    try:
        with open(file_path, "rb") as f:
            f.seek(header_size + start_sample * unitsize)
            raw = f.read(num_samples * unitsize)
    except Exception as exc:
        return _dumps({"error": str(exc)})

    n_got = len(raw) // unitsize
    sample_period_ns = int(1e9 / samplerate) if samplerate > 0 else 0

    # Build per-channel sample lists
    signals: dict[str, list[int]] = {}
    labels: dict[str, int] = {}   # label -> bit position in word
    for entry in ch_map:
        bit = entry.get("phys", entry.get("seq", 0))
        label = entry.get("name") or f"CH{bit}"
        signals[label] = []
        labels[label] = bit

    timestamps_ns = []
    for i in range(n_got):
        off = i * unitsize
        val = raw[off] if unitsize == 1 else struct.unpack_from("<H", raw, off)[
            0]
        timestamps_ns.append((start_sample + i) * sample_period_ns)
        for label, bit in labels.items():
            signals[label].append((val >> bit) & 1)

    # Edge detection
    edges: dict[str, list] = {}
    for label, samples in signals.items():
        ch_edges = []
        for i in range(1, len(samples)):
            if samples[i] != samples[i - 1]:
                ch_edges.append({
                    "sample": start_sample + i,
                    "time_ns": timestamps_ns[i],
                    "edge": "rising" if samples[i] == 1 else "falling",
                })
        if ch_edges:
            edges[label] = ch_edges[:20]  # cap at 20 per channel

    return _dumps({
        "file": file_path,
        "samplerate": samplerate,
        "start_sample": start_sample,
        "samples_decoded": n_got,
        "sample_period_ns": sample_period_ns,
        "trigger": meta.get("trigger", {}),
        "signals": signals,
        "edges": edges,
    })


@mcp.tool()
def signal_summary(file_path: str) -> str:
    """Summarise signal activity across an entire capture file.

    Works for both logic analyzer and oscilloscope (DSO/analog) captures.
    Automatically detects the capture mode from the .meta.json sidecar.

    For logic captures, reports per signal:
    - Percentage of time high vs low
    - Number of transitions (edges)
    - Approximate frequency (if periodic)
    - Whether the channel is idle (stuck high or low)
    - Samplerate optimisation hints (Nyquist-based)

    For DSO/analog captures, reports per channel:
    - Min/Max voltage (mV)
    - Peak-to-peak voltage (Vpp)
    - RMS voltage
    - DC offset (mean)
    - Approximate frequency (zero-crossing based)
    - Whether channel appears idle (constant voltage, Vpp < 5 mV)
    - Samplerate optimisation hints (Nyquist-based)

    A top-level ``samplerate_hint`` is included when the capture is
    heavily oversampled relative to the fastest observed signal.

    Args:
        file_path: Path to a .bin file produced by capture_logic or capture_dso.
    """
    if not Path(file_path).exists():
        return _dumps({"error": "file not found: %s" % file_path})

    meta = _load_meta(file_path)
    samplerate = meta.get("samplerate", 1_000_000)
    ch_map = meta.get("channel_map", [])
    unitsize = meta.get("unitsize", 2)
    mode = meta.get("mode", "logic")
    header_sz = 12
    max_samp = 1_000_000

    if not meta:
        try:
            with open(file_path, "rb") as f:
                hdr = f.read(header_sz)
            if len(hdr) == header_sz:
                samplerate, n_ch_hdr = struct.unpack("<QI", hdr)
                ch_map = [{"seq": i, "phys": i, "name": "CH%d" % i}
                          for i in range(n_ch_hdr)]
                unitsize = 2 if n_ch_hdr > 8 else 1
        except Exception:
            pass

    if not ch_map:
        return _dumps({"error": "cannot determine channel map"})

    # Dispatch to DSO-specific analysis
    if mode in ("dso", "analog"):
        return _signal_summary_dso(file_path, meta, samplerate, ch_map,
                                   header_sz, max_samp)

    # --- Logic mode analysis ---
    labels = {}
    for entry in ch_map:
        bit = entry.get("phys", entry.get("seq", 0))
        label = entry.get("name") or ("CH%d" % bit)
        labels[label] = bit

    try:
        with open(file_path, "rb") as f:
            f.seek(header_sz)
            raw = f.read(unitsize * max_samp)
    except Exception as exc:
        return _dumps({"error": str(exc)})

    n_samples = len(raw) // unitsize
    if n_samples == 0:
        return _dumps({"error": "no sample data in file"})

    high_count = {lbl: 0 for lbl in labels}
    transitions = {lbl: 0 for lbl in labels}
    prev = {lbl: None for lbl in labels}

    for i in range(n_samples):
        off = i * unitsize
        val = raw[off] if unitsize == 1 else struct.unpack_from("<H", raw, off)[
            0]
        for label, bit in labels.items():
            b = (val >> bit) & 1
            high_count[label] += b
            if prev[label] is not None and b != prev[label]:
                transitions[label] += 1
            prev[label] = b

    summary = {}
    max_signal_freq = 0.0
    for label, bit in labels.items():
        pct = 100.0 * high_count[label] / n_samples
        tr = transitions[label]
        freq = None
        if tr >= 2 and samplerate > 0:
            freq = round((tr / 2.0) / (n_samples / samplerate), 1)

        entry_d: dict = {
            "pct_high": round(pct, 1),
            "transitions": tr,
        }
        if high_count[label] == n_samples:
            entry_d["state"] = "always HIGH (idle)"
        elif high_count[label] == 0:
            entry_d["state"] = "always LOW (idle)"
        elif freq is not None:
            entry_d["approx_freq_hz"] = freq
            max_signal_freq = max(max_signal_freq, freq)
            if samplerate > 0 and freq > 0:
                oversample = samplerate / freq
                min_rate = freq * 4
                rec_rate = freq * 10
                entry_d["min_samplerate_hz"] = round(min_rate)
                entry_d["rec_samplerate_hz"] = round(rec_rate)
                entry_d["oversampling_ratio"] = round(oversample, 1)
                if oversample > 20:
                    entry_d["hint"] = (
                        "Oversampled %.0fx. "
                        "Recommended %s (10x) -- "
                        "would allow %.0fx longer capture."
                        % (oversample, _fmt_rate(rec_rate),
                           samplerate / rec_rate)
                    )

        summary[label] = entry_d

    result: dict = {
        "file": file_path,
        "mode": "logic",
        "samples_analysed": n_samples,
        "samplerate": samplerate,
        "trigger": meta.get("trigger", {}),
        "signals": summary,
    }
    if max_signal_freq > 0 and samplerate > 0:
        rec = max_signal_freq * 10
        oversample = samplerate / max_signal_freq
        if oversample > 20:
            result["samplerate_hint"] = (
                "Fastest signal ~%s. Current samplerate %s (%.0fx oversample). "
                "Recommended: %s (10x Nyquist) -- "
                "would allow %.0fx longer capture at same memory depth."
                % (_fmt_rate(max_signal_freq), _fmt_rate(samplerate),
                   oversample, _fmt_rate(rec), samplerate / rec)
            )

    return _dumps(result)


def _signal_summary_dso(file_path: str, meta: dict, samplerate: int,
                        ch_map: list, header_sz: int,
                        max_samp: int) -> str:
    """Analog/DSO signal summary -- voltage statistics per channel."""
    n_ch = len(ch_map)
    # DSO: interleaved 8-bit samples, 1 byte per channel
    read_bytes = n_ch * max_samp

    try:
        with open(file_path, "rb") as f:
            f.seek(header_sz)
            raw = f.read(read_bytes)
    except Exception as exc:
        return _dumps({"error": str(exc)})

    n_samples = len(raw) // n_ch
    if n_samples == 0:
        return _dumps({"error": "no sample data in file"})

    summary = {}
    max_signal_freq = 0.0

    for seq, ch_entry in enumerate(ch_map):
        label = ch_entry.get("name") or ("CH%d" % ch_entry.get("phys", seq))
        vdiv = ch_entry.get("vdiv_mV", 1000)
        pfact = ch_entry.get("probe_factor", 1)
        hw_off = ch_entry.get("hw_offset", 128)

        # Convert raw samples to voltages and compute stats in one pass
        v_sum = 0.0
        v_sq_sum = 0.0
        v_min = float("inf")
        v_max = float("-inf")
        prev_v = None
        crossings = 0

        # First pass: compute mean for zero-crossing reference
        raw_sum = 0
        for i in range(n_samples):
            raw_sum += raw[i * n_ch + seq]
        raw_mean = raw_sum / n_samples
        mean_v = _raw_to_voltage_mV(int(round(raw_mean)), vdiv, pfact, hw_off)

        # Second pass: full statistics
        for i in range(n_samples):
            sample = raw[i * n_ch + seq]
            v = _raw_to_voltage_mV(sample, vdiv, pfact, hw_off)
            v_sum += v
            v_sq_sum += v * v
            if v < v_min:
                v_min = v
            if v > v_max:
                v_max = v
            if prev_v is not None:
                if (prev_v - mean_v) * (v - mean_v) < 0:
                    crossings += 1
            prev_v = v

        v_mean = v_sum / n_samples
        v_rms = math.sqrt(v_sq_sum / n_samples)
        v_pp = v_max - v_min

        ch_result: dict = {
            "vdiv_mV": vdiv,
            "probe_factor": pfact,
            "coupling": ch_entry.get("coupling", "DC"),
            "full_scale_mV": vdiv * pfact * 10,
            "min_mV": round(v_min, 2),
            "max_mV": round(v_max, 2),
            "vpp_mV": round(v_pp, 2),
            "vrms_mV": round(v_rms, 2),
            "dc_offset_mV": round(v_mean, 2),
        }

        # Idle detection: Vpp < 5 mV means effectively constant
        if v_pp < 5.0:
            ch_result["state"] = "idle (constant ~%.1f mV)" % v_mean

        # Frequency estimation
        freq = None
        if crossings >= 2 and samplerate > 0:
            freq = round((crossings / 2.0) / (n_samples / samplerate), 1)

        if freq is not None:
            ch_result["approx_freq_hz"] = freq
            max_signal_freq = max(max_signal_freq, freq)
            if samplerate > 0 and freq > 0:
                oversample = samplerate / freq
                min_rate = freq * 4
                rec_rate = freq * 10
                ch_result["min_samplerate_hz"] = round(min_rate)
                ch_result["rec_samplerate_hz"] = round(rec_rate)
                ch_result["oversampling_ratio"] = round(oversample, 1)
                if oversample > 20:
                    ch_result["hint"] = (
                        "Oversampled %.0fx. "
                        "Recommended %s (10x) -- "
                        "would allow %.0fx longer capture."
                        % (oversample, _fmt_rate(rec_rate),
                           samplerate / rec_rate)
                    )

        summary[label] = ch_result

    result: dict = {
        "file": file_path,
        "mode": meta.get("mode", "dso"),
        "samples_analysed": n_samples,
        "samplerate": samplerate,
        "trigger": meta.get("trigger", {}),
        "signals": summary,
    }

    if max_signal_freq > 0 and samplerate > 0:
        rec = max_signal_freq * 10
        oversample = samplerate / max_signal_freq
        if oversample > 20:
            result["samplerate_hint"] = (
                "Fastest signal ~%s. Current samplerate %s (%.0fx oversample). "
                "Recommended: %s (10x Nyquist) -- "
                "would allow %.0fx longer capture at same memory depth."
                % (_fmt_rate(max_signal_freq), _fmt_rate(samplerate),
                   oversample, _fmt_rate(rec), samplerate / rec)
            )

    return _dumps(result)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    mcp.run()
