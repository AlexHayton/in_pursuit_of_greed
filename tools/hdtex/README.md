# hdtex — upscaled art for In Pursuit of Greed

Extracts every graphic lump from `GREED.BLO`, upscales it 4× with Real-ESRGAN, requantizes to the
game's own 256-colour VGA palette, and packs the result into a sidecar archive the engine loads
alongside the original. The 39 FLI cutscenes are a separate job, upscaled 2× with NVIDIA's VSR.

The sidecar is optional. Without it the game is unchanged; with it, the existing **RENDERER** option
selects the art set — `ORIGINAL` uses the original lumps, `HD` uses the upscaled ones.

## Use

```powershell
.\setup.ps1                                 # venv + torch + weights + NVEncC (~2.5 GB)
.\.venv\Scripts\python roundtrip.py         # prove the codecs before trusting anything
.\.venv\Scripts\python nvvsr.py --selftest  # prove the VSR colour path
.\.venv\Scripts\python make_hd.py --all     # -> greed_final\GREED_HD.001.BLO ...
.\.venv\Scripts\python make_fli.py          # -> greed_final\MOVIES  (~40 min)
```

macOS: `./setup.sh`, then the same commands through `.venv/bin/python` — except the cutscenes.
The VSR backends need an NVIDIA GPU, so on macOS `make_fli.py` has to be run with
`--backend esrgan`, or the files generated on Windows taken as they are. They are ordinary data
either way; nothing in the engine cares which produced them.

Iterating on one class:

```powershell
.\.venv\Scripts\python make_fli.py --only ARBITER      # cutscenes, separate step
.\.venv\Scripts\python fli_compare.py --only CITYBURN  # backends side by side
.\.venv\Scripts\python make_hd.py --only flat --model x4plus_anime
.\.venv\Scripts\python contact_sheet.py flat        # look at it before packing
.\.venv\Scripts\python pack.py --only wall,flat     # partial packs are fine
```

## Cutscene backends

`make_fli.py --backend` picks between three upscalers. `fli_compare.py` renders them side by side
on the same frames, requantized to the palette, which is the only fair comparison.

| | what it is | needs |
|---|---|---|
| `ngx-vsr` | NGX DLVSR — the model behind RTX Video Super Resolution. **The default.** `--quality 1..4` | nothing; `nvngx_vsr.dll` ships inside the NVEncC archive `fetch_nvencc.py` downloads |
| `nvvfx-superres` | Maxine Video Effects SuperRes. `--mode 0` conservative, `1` aggressive | the Video Effects runtime — see below |
| `esrgan` | Real-ESRGAN, which the texture pack still uses | the weights `fetch_weights.py` downloads |

There is no Python binding for any of this. NVEncC is the way in: it wraps both NVIDIA networks as
`--vpp-resize` filters, is a portable archive with no installer, and does raw y4m in and out, so a
whole movie is one invocation. The alternative was a bespoke D3D11 app driving the video
processor's NVIDIA extension — the path Chromium and mpv use for the browser feature — which is
documented for 360p–1440p input and would have refused 320×200 anyway.

### Maxine Video Effects, for `nvvfx-superres` only

NVEncC looks for `C:\Program Files\NVIDIA Corporation\NVIDIA Video Effects\NVVideoEffects.dll` and
says so if it is missing. Getting it needs an NGC account: create one, generate an API key, fetch
the Maxine Video Effects SDK for Windows from NGC, and run its `install_feature.ps1`. `ngx-vsr`
needs none of that, which is most of why it is the default.

`--resume` skips lumps already produced, so changing the model for one class does not redo the
others. Output is deterministic: same input, weights and flags give a byte-identical pack.

## The pack is split across five files

GitHub refuses a blob over 100 MB and warns over 50; the pack is 209 MB. `pack.py` therefore writes
`GREED_HD.001.BLO` … `GREED_HD.005.BLO`, about 43 MB each, and `CA_OverlayArt` loads whatever it
finds. `--part-mb` sets the budget, and `--part-mb 0` writes one undivided `GREED_HD.BLO` — still
supported, still loaded if present, but gitignored because it cannot be pushed.

The split needed no format change. A part is simply a *partial* pack — the same shape
`pack.py --only wall` already produced for iterating on one class — so it spans the full lump
number space with size 0 meaning "not mine". Any subset loads; lumps the missing parts would have
supplied just come from the original art.

Two things about that are easy to get wrong, and both were:

**Every part carries the whole build's header values, not its own.** `hudscale` is only set when
pics *and* fonts are present, and those can land in different parts — computed per part, neither
would have claimed it. The engine merges by max, so agreeing values are what make the merge right.

**A part number is not a flag.** `lumpsrc[]` holds which file each lump lives in, and
`CA_SetArtMode` used to overwrite it with `1` when switching to HD. That sent every read to part 1
at an offset belonging to part 3 — which mostly succeeds and returns plausible garbage rather than
failing, so it showed up as a hang during preload, not an error. `hdpart[]` now holds the mapping
and `lumpsrc[i] = hd ? hdpart[i] : 0`.

Proof the split changed nothing: framebuffer dumps at ticks 120 and 300 are **byte-identical**
between a five-part pack and an undivided one.

## Files

| | |
|---|---|
| `blo.py` | the archive format — reader and writer |
| `fli.py` | the FLI cutscene format — reader and writer |
| `formats.py` | wall / flat / sprite / pic / font codecs, exact inverses |
| `hdformats.py` | the widened layouts 4× needs (sprites, fonts) |
| `assets.py` | which lump is which, driven by the archive's own markers |
| `palette.py` | palette expansion and the Oklab quantizer |
| `rrdbnet.py` | Real-ESRGAN's generator, vendored |
| `upscale.py` | inference, wrap-aware tiling, mask handling |
| `nvvsr.py` | the NVIDIA backends: NVEncC driver, y4m transport, colour path |
| `fetch_nvencc.py` | downloads and unpacks NVEncC |
| `fli_compare.py` | cutscene upscalers side by side, post-quantization |
| `extract.py` → `process.py` → `pack.py` | the three stages |
| `make_hd.py` | driver |
| `roundtrip.py` | decode+re-encode every lump and compare |
| `contact_sheet.py` | before/after grids |
| `make_fli.py` | upscale the 39 cutscenes -> `greed_final/MOVIES` |
| `shots.ps1` | capture framebuffer dumps in both render modes |

## Things that matter

**Round-trip first.** `roundtrip.py` decodes and re-encodes all 2141 graphic lumps and rebuilds the
whole archive, comparing byte for byte. It passes. Any format change should keep it passing — it is
much stronger evidence than looking at output.

**Walls and flats tile.** They wrap on both axes at draw time, so they are upscaled as a 3×3
arrangement of themselves and the middle ninth is cropped back out. Upscaling a tile in isolation
puts a hard seam at every 64-unit boundary — on a corridor wall, a visible line every tile.

**Sprites need their transparency filled before upscaling.** Feeding index-0 black to the network
makes it inpaint a dark halo around every sprite. The transparent region is flooded with nearby
opaque colour first, the opacity mask is upscaled separately and re-thresholded, then reapplied.

**A sprite column stores interior transparent pixels.** The span from first to last opaque row is
stored whole, and `ScaleMaskedPost` skips zeros at draw time — so "inside the stored span" is not the
same question as "nonzero", and the span has to be preserved separately to re-encode exactly. That is
what the `.mask.png` files are.

**Full-screen art has its own palette.** Every 320×200 pic is followed in the archive by a 768-byte
palette lump, and those screens are drawn under it rather than the game palette. Quantizing them
against lump 91 wrecks them.

**Fonts are not palette-indexed.** Glyph bytes are offsets added to `fontbasecolor` at draw time, so
they are upscaled as a ramp and rescaled back into their original value range, never colour-matched.

**No dithering.** It fights the upscaler's gradients, and since the 64 colormaps remap every index
per-frame by depth, a dither pattern becomes crawling noise rather than averaging out.

**Status bar indices are markers, not colours.** The art paints index 254 across the meter regions
and the engine rewrites 254 — or a 113..168 value it wrote last frame — with a gradient from the
player's shield and health. Upscaling destroyed all 524 markers and invented spurious ones, so those
pixels are replicated 4× as *indices* and the ranges are excluded from the quantizer elsewhere.

**Index 0 is transparent in masked pics too**, not just sprites — the weapon, cursor, heart and menu
sliders. They get the same fill-upscale-remask treatment.

**The FLI sign conventions are opposite to each other**, and both are easy to
invert into something that looks almost right: in BRUN a negative count means
literal bytes and positive means a run; in LC it is the other way round. The
engine's per-row packet counter is a byte, so the writer caps packets at 255 and
falls back to BRUN, then to an uncompressed COPY chunk, if a row will not fit.
`fli.py` round-trips the original movies byte-for-byte at within 0.1% of their
original size, which is what makes it safe to re-encode them.

**Cutscene frames are upscaled independently.** None of the three backends has a temporal model —
including NGX VSR, despite the name; it is a per-frame network like the others. So a texture the
eye tracks across a pan can shimmer slightly, and switching to VSR did not change that. Fixing it
properly (optical flow, or keyframes plus warping) is a much bigger job than a five-second cutscene
justifies.

**ngx-vsr emits limited-range YUV whatever `--colorrange` says.** The flag only tags metadata. Fed
full-range samples the filter clamps the ends — 0 → 16 and 255 → 235, mid-tones untouched — which
crushes every pure black and pure white in a cutscene. Fed limited-range samples the transfer is
exact. The transport is therefore 10-bit limited-range 4:4:4 smpte170m: 4:4:4 because the raw
reader has no RGB input and 4:2:0 would halve the chroma of art that is mostly saturated flat
fills, and 10-bit because limited range at 8 bits has only 219 levels for 256 values. At that
setting `nvvsr.py --selftest` measures the round trip as exactly lossless — max error 0, zero bias.
A grey ramp is what caught this and it is now part of the selftest; nothing else here would have.

**A comparison against a good resampler is the honest one.** Once the levels bug was fixed, ngx-vsr
differs from plain `lanczos4` by a mean absolute difference of only about 1.4. Most of the
difference that appeared to be "the model working" beforehand was the clamp. The selftest still
checks that number, but only to tell "ran" from "did not run" — NVEncC accepts scale factors the
networks do not support and silently substitutes an ordinary resize, so `_assert_ran` reads back
its own reported filter chain as the real guard.

**Delta compression is not always the win.** The writer encodes each frame both
ways and keeps the smaller: on high-motion content LC degenerates into literals
plus a skip byte per span and loses to a plain RLE of the whole frame. One
cutscene came out at 519 KB/frame before this, five times its neighbours.

**An interrupted movie is detectably invalid, and `--resume` checks.** The FLI
header carries the frame count and total size, so it can only be written at
close; a run killed part way through leaves a file with a zeroed signature.
`--resume` therefore validates each output against its source frame count rather
than testing for existence, or a truncated file would be skipped for ever.

**VSR costs about twice the disk of ESRGAN, and it is worth it.** Measured whole-movie at 2×:

| | original | esrgan | ngx-vsr |
|---|---|---|---|
| ARBITER | 0.43 MB | 1.19 MB | 2.53 MB |
| TEXT | 0.89 MB | 3.63 MB | 7.12 MB |
| CITYBURN | 1.65 MB | 10.43 MB | 18.66 MB |
| all 39 | 43 MB | 156 MB | **283 MB** |

The reason is the reason to prefer it. ESRGAN smooths, so its output RLEs well; VSR preserves the
source's dither and banding, which is exactly what FLI's encoder cannot compress. Going in, the
smaller set looked like the argument *for* trying VSR. It is the argument against, and it loses to
what the smoothing costs:

**ESRGAN deletes fine detail and invents geometry.** On `TEXT.FLI` it erases the entire starfield —
not thinned, gone, a pure black background both in the sheets and in the framebuffer dumps from a
running game. On `CITYBURN` at a tight crop it restructures the skyline: towers merge and a thin one
disappears into a dark mass. `ngx-vsr` keeps both faithfully, at about 17% more lit pixels on the
same frame. That is why the cutscenes moved and the texture pack did not — a wall tile has no
starfield to lose, and `upscale.wrap()`'s seam handling has no equivalent in a video filter chain.

For reference, re-encoding the same frames as H.264 measures 7x smaller at
CRF 16, 9.5x at CRF 18 and 12.5x at CRF 20. The engine cannot play that back: its cutscene path is
8-bit paletted end to end, so it would need a decoder (libavcodec, or Media Foundation plus
VideoToolbox) and a present path that bypasses `screen` and the palette LUT entirely.

## Status

| | | |
|---|---|---|
| walls / flats / sprites / pics / fonts | 2141 lumps, 4× ESRGAN | `GREED_HD.001.BLO`…`.005.BLO`, 209 MB |
| backdrop / sky | 4 lumps, 4× ESRGAN | in the same sidecar, 1024×1024 buffer |
| cutscenes | 39 movies, 5722 frames, 2× NGX VSR | `greed_final/MOVIES/`, 283 MB |

640×400 is the only cutscene size now; the 4× set and the `--scale 4` default are gone. It
point-doubles into the 1280×800 HD chrome and point-samples down into the original renderer's
320×200, both in `VI_BlitLogical`.

Both are committed, so a clone is playable as-is — which matters because the cutscene backend needs
an NVIDIA GPU and macOS cannot rebuild those files. Both remain optional: the game runs unchanged
without either, and `-noHD` skips the pack outright.
