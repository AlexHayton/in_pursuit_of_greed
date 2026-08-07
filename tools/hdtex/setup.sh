#!/bin/sh
# Creates the virtualenv this pipeline runs in, and fetches the Real-ESRGAN weights.
# Safe to re-run.  macOS gets the plain PyPI torch wheel, which carries the MPS
# backend; upscale.py picks MPS over CPU automatically.
#
#   ./setup.sh              # torch from PyPI (MPS on Apple silicon)
#   ./setup.sh --no-weights # environment only

set -eu
cd "$(dirname "$0")"

VENV=.venv
PY="$VENV/bin/python"

if [ ! -x "$PY" ]; then
    # Pin to a version torch ships wheels for; the newest installed python3 may
    # be too new (a 3.15 alpha broke this on the Windows side).
    CHOSEN=
    for v in 3.13 3.12 3.11 3.10; do
        if command -v "python$v" >/dev/null 2>&1; then CHOSEN="python$v"; break; fi
    done
    if [ -z "$CHOSEN" ]; then
        echo "no suitable Python found; torch publishes wheels for 3.10-3.13" >&2
        exit 1
    fi
    echo "==> creating $VENV on $CHOSEN"
    "$CHOSEN" -m venv "$VENV"
else
    echo "==> $VENV already exists"
fi

"$PY" -m pip install --quiet --upgrade pip
echo "==> installing numpy / pillow / tqdm"
"$PY" -m pip install --quiet -r requirements.txt

if "$PY" -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('torch') else 1)"; then
    echo "==> torch already installed"
else
    echo "==> installing torch"
    "$PY" -m pip install torch
fi

if [ "${1:-}" != "--no-weights" ]; then
    echo "==> fetching Real-ESRGAN weights"
    "$PY" fetch_weights.py
fi

echo
"$PY" -c "import torch;print('torch',torch.__version__,'mps',torch.backends.mps.is_available())"
echo
echo "ready.  next:"
echo "    $PY roundtrip.py      # prove the codecs"
echo "    $PY make_hd.py --all  # build GREED_HD.BLO"
