# Writing your DBC file

A DBC file is plain text. Each CAN message is a `BO_` line, and each signal inside it is an indented `SG_` line.

```
BO_ <id_decimal> <MessageName>: <length_bytes> <sender>
 SG_ <SignalName> : <start_bit>|<length_bits>@<endian><sign> (<scale>,<offset>) [<min>|<max>] "<unit>" <receiver>
```

Working example (this exact file is `sample_data/sample_test.dbc` and is verified by the tools):

```
BO_ 256 RPM_STATUS: 8 ECU
 SG_ EngineRPM : 0|16@1+ (0.25,0) [0|16000] "rpm" DASH
```

## Rules that trip people up

- **Do not use `/* ... */` or `//` comments.** The DBC format does not support them and cantools will refuse to load the file. Use `CM_ "text";` lines instead.
- **Decimal IDs.** The log shows hex, DBC wants decimal. `0x1A0` becomes `416`. Python: `int("1A0", 16)`.
- **Extended (29-bit) IDs.** `analyze_log.py` tells you if any exist. In the DBC use `ID + 2147483648` (that is, `ID | 0x80000000`).
- **Signals must not overlap** inside one message - cantools rejects the file if two signals share bits.
- **Sender / receiver names** must be declared on the `BU_:` line at the top (the template has `ECU` and `DASH`).

## Start bit and endianness

Bits are numbered 0-63 across the 8 data bytes: byte 0 holds bits 0-7, byte 1 holds bits 8-15, and so on.

| You see | Use |
|---|---|
| One byte, e.g. byte 0 | `0\|8@1+` |
| One byte, e.g. byte 1 | `8\|8@1+` |
| 16-bit value, **low byte first** (byte 0 = low, byte 1 = high) | `0\|16@1+` (Intel, `@1`) |
| 16-bit value, **high byte first** (byte 0 = high, byte 1 = low) | `7\|16@0+` (Motorola, `@0`) |
| Single bit, bit 0 of byte 0 | `0\|1@1+` |
| Two bits at bits 1-2 of byte 0 | `1\|2@1+` |

If a 2-byte signal decodes to nonsense, flip between `@1` and `@0` (with the matching start bit) and re-run `decode_log.py`.

## Working out scale and offset

`real_value = raw * scale + offset`

Take two points where you know the real value (read it off the dash) and note the raw value from the log:

```
scale  = (real2 - real1) / (raw2 - raw1)
offset = real1 - raw1 * scale
```

Example: dash showed 2000 rpm when raw was 8000, and 4000 rpm when raw was 16000, so `scale = 2000 / 8000 = 0.25` and `offset = 2000 - 8000 * 0.25 = 0`.

This is why the logging protocol in the README has you hold steady RPM values for a few seconds and press a marker at each one.

## Validate

```
python tools/decode_log.py canlog_001.csv dbc/dominar.dbc -o decoded_001.csv
```

Open `decoded_001.csv` (or plot it) and compare against what the dash showed at the marker times. If it matches at several points, the signal definition is right.
