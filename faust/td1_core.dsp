// FORESIGHT TD-1 — core engine (matches web prototype v2 kernel)
// Anticipatory transient designer: ratio-normalized differential detection,
// lookahead, transient-only tilt, attack/sustain gain law, delta, soft clip.
// Build (macOS):  faust2juce -jucemodulesdir <path> td1_core.dsp   (then build AU/VST3)
// Quick test:     paste into https://faustide.grame.fr
declare name "Foresight TD-1 Core";
declare version "0.2";
declare author "Renky Denk / Black Tie Ventures";
declare license "Proprietary — all rights reserved";

import("stdfaust.lib");

//---------------------------------- parameters
attackDb  = hslider("[00]Attack [unit:dB]",   6,  -24, 24, 0.1);
sustainDb = hslider("[01]Sustain [unit:dB]",  0,  -24, 24, 0.1);
speed     = hslider("[02]Detector Speed",     1,  0.5, 2,  0.01);
schpf     = hslider("[03]Detector HPF [unit:Hz]", 60, 20, 500, 1);
tiltDb    = hslider("[04]Transient Tilt [unit:dB]", 0, -12, 12, 0.1);
lookMs    = hslider("[05]Lookahead [unit:ms]", 3, 0, 10, 0.1);
mix       = hslider("[06]Mix", 100, 0, 100, 1) / 100.0;
outG      = hslider("[07]Output [unit:dB]", 0, -12, 12, 0.1) : ba.db2linear;
clipOn    = checkbox("[08]Soft Clip");
linkOn    = checkbox("[09]Stereo Link");
deltaOn   = checkbox("[10]Delta");
bypassOn  = checkbox("[11]Bypass");

//---------------------------------- helpers
T = 1.0 / max(0.25, speed);            // time-constant scale
eps = 1e-4;
clamp01(x) = max(0, min(1, x));
mytanh(x) = (e2 - 1) / (e2 + 1) with { xc = max(-10, min(10, x)); e2 = exp(2*xc); };

// asymmetric one-pole envelope follower on |x| (att/rel in seconds)
envUD(att, rel) = abs : an.amp_follower_ud(att, rel);

//---------------------------------- detector (per detection signal d)
wAraw(d) = clamp01((eAF - eAS) / (eAF + eps))
with {
    eAF = d : envUD(0.00008, 0.030*T);
    eAS = d : envUD(0.012*T, 0.030*T);
};
wSraw(d) = clamp01((eSS - eSF) / (eSS + eps))
with {
    eSF = d : envUD(0.0005, 0.045*T);
    eSS = d : envUD(0.0005, 0.450*T);
};
detW(d) = wAraw(d) : si.onePoleSwitching(0.0008, 0.0008);
detG(d) = (attackDb*wAraw(d) + sustainDb*wSraw(d))
          : ba.db2linear : si.onePoleSwitching(0.00012, 0.006);

//---------------------------------- per-channel output stage
lookSamps = int(lookMs * 0.001 * ma.SR);
dline(x) = x : de.delay(4096, lookSamps);   // 4096 covers 10 ms up to ~192 kHz... see note in README

gHi = ba.db2linear(tiltDb * 0.5);
gLo = ba.db2linear(0 - tiltDb * 0.5);

shape(xd, g, wAs) = y
with {
    lp     = xd : fi.lowpass(1, 750);
    tilted = lp*gLo + (xd - lp)*gHi;
    xt     = xd + (tilted - xd)*wAs;
    wet    = xt * g;
    ymix   = xd + (wet - xd)*mix;
    ydel   = select2(deltaOn, ymix, ymix - xd);
    yclip  = select2(clipOn, ydel, mytanh(ydel));
    y      = select2(bypassOn, yclip * outG, xd);
};

//---------------------------------- stereo process
process(l, r) = yl, yr
with {
    hl = l : fi.highpass(1, schpf);
    hr = r : fi.highpass(1, schpf);
    dLink = max(abs(hl), abs(hr));
    dL = select2(linkOn, abs(hl), dLink);
    dR = select2(linkOn, abs(hr), dLink);
    yl = shape(dline(l), detG(dL), detW(dL));
    yr = shape(dline(r), detG(dR), detW(dR));
};
