# BowleRLE

Decompress the .ANI and .SLD files from your Brunswick bowling computer. You have one of those, right?

## Usage

```
bowlerle input.ani [output.png]
```

## Building

Exact steps will vary depending on your build environment, but `pkg-config` or `pkgconf` should make it easy.
Also, you need libpng since the output format is PNG.

```
gcc -Os -o bowlerle bowlerle.c `pkg-config --cflags --libs libpng`
```

## File Format

This is reverse-engineered, so the details may not be correct. Values are little-endian integers unless specified otherwise.

| Offset | Data |
| --- | --- |
| `0x00` - `0x03` | Magic number `2A 41 4E 49` = `*ANI` |
| `0x04` - `0x0B` | *Unknown* |
| `0x0C` - `0x0F` | Number of frames |
| `0x10` - `0x13` | *Unknown* |
| `0x14` - `0x17` | Offset of palette |
| `0x18` - `0x1B` | Offset of image offset table |
| `0x1C` - `0x23` | *Unknown* |
| `0x24` - `0x27` | Decoded width, in 320-byte units |
| `0x28` - `0x2B` | Displayed width |
| `0x2C` - `0x2F` | Height |
| `0x30` - ... | *Unknown* |

The palette is 256 entries of 3 bytes. Each entry is one byte for red, one byte for green, and then one byte for blue.
Only the lower 6 bits of each byte are used; the upper bits are ignored.
It's probably not a coincidence that this matches the format used by the VGA DAC.

The image offset table is a series of four-byte integers indicating the offset of each frame's compressed data within the file.

### Compression

The compressed data is a series of commands. Each command is one or two bytes.
Except for the end-of-data command, all commands are followed by one or more bytes of data.
Bits 7 and 6 determine the type and length of each command, as follows:

| Command | Meaning |
| `0b00xxxxxx` | One-byte command, compressed run (or end-of-data) |
| `0b01xxxxxx 0bxxxxxxxx` | Two-byte command, compressed run |
| `0b10xxxxxx` | One-byte command, literal copy |
| `0b11xxxxxx 0bxxxxxxxx` | Two-byte command, literal copy |

Each command encodes a length in the remaining bits. This length indicates how many bytes will be written to the output.
One-byte commands simply encode the length in the lower six bits.
For two-byte commands, the lower six bits of the first byte are the upper six bits of the 14-bit length, and the entire second byte is the lower eight bits of the length.

Commands that indicate a compressed run are followed by one byte. When this byte is nonzero, it's the value repeated in the output.
When this byte is zero, it indicates data copied from the same location in the previous frame.
Commands that indicate a literal copy are followed by one or more bytes that are directly copied to the output.
It's unknown whether zero has special meaning for literal copies.

The end-of-data command is the value `0x00`. It's unknown whether any other command with a length of zero would also terminate decompression.
