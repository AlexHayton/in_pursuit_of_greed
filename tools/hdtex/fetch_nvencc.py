"""Download and extract NVEncC, which is how this pipeline reaches NVIDIA's
super-resolution networks.

There is no Python binding for RTX VSR.  The alternatives were a bespoke D3D11
app driving the video processor's NVIDIA extension -- the path Chromium and mpv
use for the browser feature, and which is documented for 360p-1440p input, so
320x200 is below its floor -- or NVIDIA's own SDKs, which need an NGC account.
rigaya's NVEncC wraps both offline networks as resize filters:

  --vpp-resize algo=ngx-vsr,vsr-quality=1..4        NGX DLVSR, the RTX VSR model
  --vpp-resize algo=nvvfx-superres,superres-mode=0|1  Maxine Video Effects SuperRes

and is a portable archive with no installer.  `nvngx_vsr.dll` ships inside it,
so ngx-vsr needs nothing else; nvvfx-superres additionally needs the Maxine
models, which do require an NGC account (see README).

The archive is 7z and there is no 7-Zip on this machine.  Windows' own
`tar.exe` is bsdtar/libarchive and reads it; `Expand-Archive` does not, and
py7zr does not either -- rigaya's archive uses the BCJ2 filter, which py7zr
declines.  So extraction shells out to tar rather than adding a dependency.
"""

import hashlib
import subprocess
import sys
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
BIN = HERE / "bin"

VERSION = "9.30"
ARCHIVE = f"NVEncC_{VERSION}_x64.7z"
URL = f"https://github.com/rigaya/NVEnc/releases/download/{VERSION}/{ARCHIVE}"
SHA256 = "6b17bdeb990cd63b731634f0ccdf7f7e7275723d75065353842b021d21832ba8"

EXE = "NVEncC64.exe"


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def exe_path(fetch=True):
    """Local path of NVEncC64.exe, downloading and extracting if needed."""
    dest = BIN / EXE
    if dest.exists():
        return dest
    if not fetch:
        raise FileNotFoundError(
            f"{dest} not found.  Run: python fetch_nvencc.py")
    _install()
    return dest


def _install():
    BIN.mkdir(exist_ok=True)
    archive = BIN / ARCHIVE

    if archive.exists() and _sha256(archive) != SHA256:
        print(f"  {ARCHIVE}: checksum mismatch, re-downloading", file=sys.stderr)
        archive.unlink()

    if not archive.exists():
        print(f"  downloading {ARCHIVE} ...")
        tmp = archive.with_suffix(".part")
        with urllib.request.urlopen(URL) as r, open(tmp, "wb") as f:
            total = int(r.headers.get("content-length", 0))
            done = 0
            while chunk := r.read(1 << 20):
                f.write(chunk)
                done += len(chunk)
                if total:
                    print(f"\r    {done * 100 // total:3d}%  "
                          f"{done >> 20}/{total >> 20} MB", end="", flush=True)
        print()
        got = _sha256(tmp)
        if got != SHA256:
            tmp.unlink()
            raise RuntimeError(f"{ARCHIVE}: sha256 {got}, expected {SHA256}")
        tmp.rename(archive)

    print(f"  extracting into {BIN} ...")
    # The archive is flat -- every DLL sits beside NVEncC64.exe, which is how it
    # expects to find them, so extract in place rather than into a subdirectory.
    r = subprocess.run(["tar", "-xf", str(archive), "-C", str(BIN)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"tar failed to extract {ARCHIVE}:\n{r.stderr}")


def main():
    if sys.platform != "win32":
        # macOS has no NVIDIA GPU, so the VSR backends cannot run there at all;
        # that port builds the cutscenes with the ESRGAN backend or takes the
        # files generated here.
        print("  NVEncC is Windows-only; nothing to do", file=sys.stderr)
        return 0
    p = exe_path()
    print(f"  NVEncC {VERSION}  {p}  {p.stat().st_size >> 20} MB  ok")
    ngx = p.parent / "nvngx_vsr.dll"
    print(f"  ngx-vsr model {'present' if ngx.exists() else 'MISSING'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
