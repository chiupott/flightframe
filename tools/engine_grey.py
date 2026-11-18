"""Grey version of the engine picture: same 3D render, same chunky Bayer pixels, but the dots come in 4 tones (paper, light grey, dark grey, black).
Usage: engine_grey.py [preview.png]   -> prints stats; bake_engine.py imports grey_levels()"""
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
import engine3d
import scipy.ndimage as ndi
def levels(ink, strength_shell=1.0, strength_black=1.0, aa=True):
    X = engine3d.EXTRA; shf, hf, black, light, thr, sil = X["shf"], X["hf"], X["black"], X["light"], X["thr"], X["sil"]
    dk = 1.0 - np.clip(shf, 0, 1)                                  # darkness 0..1 from the lighting
    edge = X["edge"]; solid = hf > 0.5
    # shells (nacelle, cowl): paper in the light, light/dark grey Bayer dots in the shadow (replaces the black dots of the 1-bit picture)
    sh = np.clip((dk - 0.10) * 1.7 * strength_shell, 0, 1) * 2.0
    shell = np.clip(np.floor(sh + thr), 0, 2)
    # solid parts (blades, core, shaft ...): black where in shadow, dark grey where lit (still ink only where the 1-bit picture had ink, so the white outlines between parts stay)
    bl = 3.0 - np.clip((0.50 - dk) * 3.0 * strength_black, 0, 1.5)
    solidlv = np.clip(np.floor(bl + thr), 2, 3)
    lv = np.zeros(ink.shape, np.uint8)
    lv = np.where(light & solid & ~edge, shell, lv)
    lv = np.where(black & ink & ~edge, solidlv, lv)
    lv = np.where(ink & (edge | sil | ~(light | black)), 3, lv)   # outlines, material edges and anything else inked: black as before
    lv = np.where(solid | sil, lv, 0).astype(np.uint8)
    if aa:                                                          # anti-alias the lines: tone follows how much of the pixel the line covers
        hf, ef = X["hf"], X["ef"]
        ring = (hf > 0.04) & ~ndi.binary_erosion(hf > 0.5, iterations=1)                       # silhouette: the outline pixel plus the partly covered pixels just outside it
        cov = np.where(hf > 0.5, 1.0, hf * 1.6)                                               # outer pixels count as partial coverage
        tone = np.where(cov > 0.80, 3, np.where(cov > 0.50, 2, np.where(cov > 0.16, 1, 0)))
        lv = np.where(ring, np.maximum(tone, np.where(hf > 0.5, 2, 0)), lv)
        lv = np.where(hf > 0.04, lv, 0)
    return lv.astype(np.uint8)
def grey_levels():
    ink, an, fill = engine3d.render((440, 300), 108, 24, -10, 2)
    return levels(ink), ink, an
if __name__ == "__main__":
    from PIL import Image
    lv, ink, an = grey_levels(); PAL = {0: 255, 1: 173, 2: 82, 3: 0}
    ys, xs = np.where(lv > 0); crop = lv[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    im = Image.fromarray(np.vectorize(PAL.get)(crop).astype(np.uint8)); print(im.size)
    im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(sys.argv[1] if len(sys.argv) > 1 else "eng_grey.png")
    lv0 = levels(ink, aa=False)[ys.min():ys.max() + 1, xs.min():xs.max() + 1]; Image.fromarray(np.vectorize(PAL.get)(lv0).astype(np.uint8)).resize((im.width * 2, im.height * 2), Image.NEAREST).save("eng_grey_old.png")
    bw = Image.fromarray(np.where(ink[ys.min():ys.max() + 1, xs.min():xs.max() + 1], 0, 255).astype(np.uint8)); bw.resize((bw.width * 2, bw.height * 2), Image.NEAREST).save("eng_bw.png")
