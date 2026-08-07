"""Real-ESRGAN x4 inference, with the wrap handling the game's textures need.

The important part of this file is not the model call, it is `wrap`.

Walls wrap on both axes at draw time -- horizontally because the column index is
masked (`texture&=63`, R_walls.c:214) and vertically because the texture
coordinate is masked against the post height (`sp_frac &= sp_loopvalue`,
R_spans.c:238) -- and flats wrap on both axes as well.  A convolutional network
run over a tile in isolation sees an artificial border on all four sides,
inpaints it as an edge, and the result is a hard seam at every 64-unit boundary:
on a corridor wall, a visible vertical line every tile.

So tiled art is upscaled as a 3x3 arrangement of itself and the middle ninth is
cropped back out.  The network then sees the correct neighbourhood across each
wrap and the seams disappear.  It costs 9x the compute, which is irrelevant here
-- the whole archive is a few minutes on any modern GPU.

Sprites and pics do not wrap; they get replicate padding instead, which stops
the model from darkening their outer rows.
"""

import numpy as np
import torch

import fetch_weights
import rrdbnet

SCALE = 4
PAD = 8              # source pixels of replicate padding for non-wrapping art


def pick_device(prefer=None):
    if prefer and prefer != "auto":
        return torch.device(prefer)
    if torch.cuda.is_available():
        return torch.device("cuda")
    if getattr(torch.backends, "mps", None) and torch.backends.mps.is_available():
        return torch.device("mps")
    return torch.device("cpu")


class Upscaler:
    def __init__(self, model="x4plus", device=None, half=None):
        spec = fetch_weights.MODELS[model]
        path = fetch_weights.path_for(model)
        self.device = pick_device(device)
        # fp16 is a clear win on CUDA and unreliable elsewhere.
        self.half = (self.device.type == "cuda") if half is None else half
        self.name = model
        self.model = rrdbnet.load(path, num_block=spec["blocks"],
                                  num_feat=spec["features"],
                                  device=self.device, half=self.half)

    def __repr__(self):
        return (f"<Upscaler {self.name} on {self.device.type}"
                f"{' fp16' if self.half else ''}>")

    # ------------------------------------------------------------------ core

    @torch.inference_mode()
    def _run(self, rgb):
        """(H,W,3) uint8 -> (4H,4W,3) uint8, straight through the net."""
        x = torch.from_numpy(np.ascontiguousarray(rgb)).to(self.device)
        x = x.permute(2, 0, 1).unsqueeze(0).float().div_(255.0)
        if self.half:
            x = x.half()
        y = self.model(x)
        y = y.float().clamp_(0, 1).mul_(255).round_()
        y = y.squeeze(0).permute(1, 2, 0).to(torch.uint8)
        return y.cpu().numpy()

    # ------------------------------------------------------------- public API

    def wrap(self, rgb, wrap_x=True, wrap_y=True):
        """Upscale art that tiles, without seams at the wrap.

        Builds a 3x3 (or 1x3 / 3x1) arrangement, upscales it, and crops the
        centre back out.  The crop is exact: the model is a fixed x4, so the
        centre tile lands at exactly (h*SCALE, w*SCALE) offset.
        """
        h, w = rgb.shape[:2]
        reps_y, reps_x = (3 if wrap_y else 1), (3 if wrap_x else 1)
        if reps_y == 1 and reps_x == 1:
            return self.pad(rgb)

        big = np.tile(rgb, (reps_y, reps_x, 1))
        out = self._run(big)
        y0 = (h * SCALE) if wrap_y else 0
        x0 = (w * SCALE) if wrap_x else 0
        return out[y0 : y0 + h * SCALE, x0 : x0 + w * SCALE]

    def pad(self, rgb, pad=PAD):
        """Upscale art that does not tile, with replicate padding at the edges."""
        if pad <= 0:
            return self._run(rgb)
        h, w = rgb.shape[:2]
        big = np.pad(rgb, ((pad, pad), (pad, pad), (0, 0)), mode="edge")
        out = self._run(big)
        p = pad * SCALE
        return out[p : p + h * SCALE, p : p + w * SCALE]

    def mask(self, mask_bool, pad=PAD):
        """Upscale a binary opacity mask and re-threshold it.

        Run through the same network as white-on-black rather than replicated
        4x, so the resulting edge follows the upscaled artwork instead of
        staying blocky.  The threshold at half hardens it again -- the engine
        has no alpha, only "index 0 or not".
        """
        rgb = np.repeat((mask_bool.astype(np.uint8) * 255)[:, :, None], 3, axis=2)
        out = self.pad(rgb, pad)
        return out.mean(axis=2) >= 128
