// FORESIGHT TD-1 — extended engine v3 (V1 feature set)
// Adds over td1_core: processing target (Stereo/Mid/Side), loudness-compensated
// auto-gain, detector audition, tilt pivot control, ADAA anti-aliased soft clip.
// Build (macOS):  faust2juce td1_v3.dsp   (then build AU/VST3)
declare name "Foresight TD-1 v3";
declare version "0.3";
declare author "Renky Denk / Black Tie Ventures";
declare license "Proprietary — all rights reserved";

import("stdfaust.lib");

//---------------------------------- parameters
attackDb  = hslider("[00]Attack [unit:dB]",   6,  -24, 24, 0.1);
sustainDb = hslider("[01]Sustain [unit:dB]",  0,  -24, 24, 0.1);
speed     = hslider("[02]Detector Speed",     1,  0.5, 2,  0.01);
schpf     = hslider("[03]Detector HPF [unit:Hz]", 60, 20, 500, 1);
tiltDb    = hslider("[04]Transient Tilt [unit:dB]", 0, -12, 12, 0.1);
pivotHz   = hslider("[05]Tilt Pivot [unit:Hz]", 750, 200, 2000, 1);
lookMs    = hslider("[06]Lookahead [unit:ms]", 3, 0, 10, 0.1);
mix       = hslider("[07]Mix", 100, 0, 100, 1) / 100.0;
outG      = hslider("[08]Output [unit:dB]", 0, -12, 12, 0.1) : ba.db2linear;
mode      = nentry("[09]Target [style:menu{'Stereo':0;'Mid':1;'Side':2}]", 0, 0, 2, 1);
clipOn    = checkbox("[10]Soft Clip (ADAA)");
linkOn    = checkbox("[11]Stereo Link");
deltaOn   = checkbox("[12]Delta");
autoOn    = checkbox("[13]Auto Gain");
audOn     = checkbox("[14]Detector Audition");
bypassOn  = checkbox("[15]Bypass");

//---------------------------------- helpers
T = 1.0 / max(0.25, speed);
eps = 1e-4;
clamp01(x) = max(0, min(1, x));
mytanh(x) = (e2 - 1) / (e2 + 1) with { xc = max(-10, min(10, x)); e2 = exp(2*xc); };
envUD(att, rel) = abs : an.amp_follower_ud(att, rel);

// ADAA-1 tanh: y = (F(x)-F(x[-1]))/(x-x[-1]), F = log(cosh)
logcosh(v) = log((exp(vc) + exp(0-vc)) * 0.5) + max(0, abs(v) - 20) with { vc = max(-20, min(20, v)); };
adaatanh(x) = select2(abs(dx) > 1e-6, mytanh(0.5*(x + x1)), (logcosh(x) - logcosh(x1)) / dx)
with { x1 = x'; dx = x - x1; };

//---------------------------------- detector
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

//---------------------------------- delay + component shaping (pre delta/clip/trim)
lookSamps = int(lookMs * 0.001 * ma.SR);
dline(x)  = x : de.delay(4096, lookSamps);
gHi = ba.db2linear(tiltDb * 0.5);
gLo = ba.db2linear(0 - tiltDb * 0.5);

shape(xd, g, wAs) = xd + (wet - xd)*mix
with {
    lp     = xd : fi.lowpass(1, pivotHz);
    tilted = lp*gLo + (xd - lp)*gHi;
    xt     = xd + (tilted - xd)*wAs;
    wet    = xt * g;
};

//---------------------------------- stereo process with M/S targeting
process(l, r) = outL, outR
with {
    // component encode
    m  = (l + r) * 0.5;
    s  = (l - r) * 0.5;
    isMS = mode > 0;
    w0 = select2(isMS, l, m);
    w1 = select2(isMS, r, s);

    // detection inputs (link applies only in stereo mode)
    h0 = w0 : fi.highpass(1, schpf);
    h1 = w1 : fi.highpass(1, schpf);
    lnk = (mode == 0) & linkOn;
    d0 = select2(lnk, abs(h0), max(abs(h0), abs(h1)));
    d1 = select2(lnk, abs(h1), max(abs(h0), abs(h1)));

    // shape flags: Mid mode shapes w0 only, Side mode shapes w1 only
    dw0 = dline(w0);  dw1 = dline(w1);
    p0  = select2(mode == 2, shape(dw0, detG(d0), detW(d0)), dw0);
    p1  = select2(mode == 1, shape(dw1, detG(d1), detW(d1)), dw1);

    // component decode back to L/R
    pL = select2(isMS, p0, p0 + p1);
    pR = select2(isMS, p1, p0 - p1);
    dL = dline(l);  dR = dline(r);

    // loudness-compensated auto-gain (RMS ~400 ms, clamped +/-12 dB)
    rmsPole = ba.tau2pole(0.4);
    msD  = (dL*dL + dR*dR) : si.smooth(rmsPole);
    msW  = (pL*pL + pR*pR) : si.smooth(rmsPole);
    corr = min(4.0, max(0.25, sqrt((msD + 1e-9) / (msW + 1e-9)))) : si.smooth(ba.tau2pole(0.2));
    cEff = select2(autoOn & (deltaOn == 0), 1.0, corr);

    // delta -> clip -> trim -> bypass / audition
    yd(x, dx_) = select2(deltaOn, x*cEff, x - dx_);
    yc(x) = select2(clipOn, x, adaatanh(x));
    fin(x, dx_) = select2(bypassOn, yc(yd(x, dx_)) * outG, dx_);
    outL = select2(audOn, fin(pL, dL), h0);
    outR = select2(audOn, fin(pR, dR), h1);
};
