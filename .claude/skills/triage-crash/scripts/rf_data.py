r"""Print the data of an RF() entry in src/fuzz/main.c as declarations.

Usage (from the repository root):

    uv run --no-project python -I \
        .claude/skills/triage-crash/scripts/rf_data.py src/fuzz/main.c <n> [--perl]

Default: one `static const unsigned char data[N]` array, for a C standalone.
--perl: a perl double-quoted string literal, `my $data = "...";`, for the
perl reproducer the report carries - byte for byte the input the test case
was given (before make_input_sv() turns it into a scalar).
"""

import re
import sys
from pathlib import Path

BYTES_PER_LINE = 12


def rf_bytes(src: str, n: str) -> bytes:
    """Return the input bytes of RF(n), checked against its size argument."""
    pattern = r"RF\(" + re.escape(n) + r',\n.*?\n("(?:\\x[0-9a-f]{2})*"),\n(\d+)\n\)'
    m = re.search(pattern, src, re.S)
    if not m:
        sys.exit(f"RF({n}) not found")
    data = bytes(int(h, 16) for h in re.findall(r"\\x([0-9a-f]{2})", m.group(1)))
    if len(data) != int(m.group(2)):
        sys.exit(f"RF({n}): size {m.group(2)} does not match {len(data)} bytes")
    return data


def c_array(name: str, b: bytes) -> str:
    lines = [
        "  " + ", ".join(f"0x{x:02x}" for x in b[i : i + BYTES_PER_LINE]) + ","
        for i in range(0, len(b), BYTES_PER_LINE)
    ]
    return f"static const unsigned char {name}[{len(b)}] = {{\n" + "\n".join(lines) + "\n};"


def perl_string(name: str, b: bytes) -> str:
    out = []
    for x in b:
        c = chr(x)
        if 0x20 <= x < 0x7F and c not in '"\\$@':
            out.append(c)
        else:
            out.append(f"\\x{x:02x}")
    return f'my ${name} = "' + "".join(out) + '";'


def main() -> None:
    """Print the declaration for RF(argv[2]) of the file argv[1]."""
    if len(sys.argv) < 3:  # noqa: PLR2004
        sys.exit(__doc__)
    data = rf_bytes(Path(sys.argv[1]).read_text(), sys.argv[2])
    if "--perl" in sys.argv[3:]:
        print(perl_string("data", data))
    else:
        print(c_array("data", data))


if __name__ == "__main__":
    main()
