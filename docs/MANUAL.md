# L8BLOOMER manual

Start with **Volume**, **Direction IN**, **Attack 500 ms**, and **Sensitivity 50%**. Each pick the detector catches starts a fresh sweep, even over amp noise or a note that’s still ringing. Turn Sensitivity up to catch smaller changes in volume, or down if it keeps triggering when you don’t want it to. LISTENING means it’s ready for another pick. WAIT means it’s still settling or waiting for the retrigger interval to pass. Place delays and reverbs after L8BLOOMER if you want their tails to continue between swells.

## Controls

| Control | What it does |
| --- | --- |
| ATTACK | 1–20000 ms, logarithmic control; whole-millisecond display |
| SENSITIVITY | 0–100%; higher values require a smaller rise above the recent input level: 12 dB at 0%, 3 dB at 50%, 0.75 dB at 100% |
| MODE | Bypass / Volume / High-pass / Low-pass / Frequency shift |
| DIRECTION | IN / OUT, shared by all active modes; never cycles or ping-pongs |
| CUTOFF | 20–20000 Hz, the open endpoint of the filter sweep; whole-Hz display |
| RESONANCE | 0.5–8 Q, default 0.707; affects filters only |
| INPUT | −24 to +24 dB, before the effect and detector |
| OUTPUT | −24 to +24 dB, after the effect |
| FROM / TO | In Frequency Shift mode, replace the two filter knobs with independent signed endpoints: −5000 to +5000 Hz, zero at 12 o'clock; both default to 0 Hz |
| MIX | Master dry/wet blend, 0–100%; default 100% wet. Input and Output trims apply to both paths |
| RETRIGGER TIME (Advanced) | 0–2000 ms minimum between accepted triggers; default 120 ms |
| TRIGGER FILTER (Advanced) | Optional detector filtering, OFF by default |
| TRIGGER LOW-CUT (Advanced) | 20–20000 Hz; rejects lower frequencies for detection only; default 120 Hz |
| TRIGGER HIGH-CUT (Advanced) | 20–20000 Hz; rejects higher frequencies for detection only; default 8000 Hz |

## Fine-tuning the trigger

You can close the Advanced section without losing its settings. Try Retrigger Time around 150–250 ms if one pick produces several triggers; shorten it for faster playing. Picks inside that interval are ignored. If you change the setting mid-note, it counts from the last trigger. At 0 ms, there’s no timed lockout, but the detector still waits for one rise to settle before accepting another.

Enable Trigger Filter to focus detection on a frequency band. Low-Cut removes rumble; raise it toward 1000 Hz to favour pick noise. High-Cut can exclude hiss above the useful band. These are 24 dB/octave Butterworth filters, not brick-wall frequency gates, and are applied only to the detector copy of the input. They do not EQ the audible signal. OFF skips their sample processing and coefficient updates entirely. Keep Low-Cut below High-Cut. If they cross, triggering pauses until you put them back in order. Bounds are limited to 45% of the sample rate internally.

## Which way does it go?

| Mode | Direction IN | Direction OUT |
| --- | --- | --- |
| Bypass | Original samples, unchanged | Original samples, unchanged |
| Volume | Silence → original input amplitude | Original input amplitude → silence |
| High-pass | 20 kHz → CUTOFF | CUTOFF → 20 kHz |
| Low-pass | 20 Hz → CUTOFF | CUTOFF → 20 Hz |
| Frequency shift | FROM → TO | TO → FROM |

## Getting metallic

Frequency Shift adds the same number of Hz to every frequency in your guitar, which makes things dissonant and metallic. It works differently from shifting by an octave or another musical interval. FROM is the left endpoint knob (CUTOFF's position) and TO is the right endpoint knob (RESONANCE's position). Older sessions keep their sweeps: BOTTOM is now FROM, and TOP is now TO. The plugin remembers these values separately from your filter settings. Either endpoint can be positive, negative, or zero, in whichever order you like. The bipolar knob scales are symmetric, with finer resolution near zero and integer-Hz values. Try FROM 0 Hz, TO +500 Hz, ATTACK 500 ms, and MIX 50%.

The shifter uses an original, optimised 20-section IIR allpass pair and quadrature modulation. The oscillator keeps running through retriggers so its phase does not jump. A 12th-order low-pass guard reduces high-frequency aliasing for upward shifts; its cutoff follows the largest positive endpoint and may soften the extreme high end. At unusually low sample rates, the effective shift is capped at one quarter of the sample rate. Negative shifts that cross zero fold into positive output frequencies, as expected for real-valued frequency shifting.

There is no FFT/block delay or lookahead, but the IIR filters have frequency-dependent phase/group delay. Zero Hz means no frequency translation, not bit-identical bypass; the wet path retains filter phase and high-frequency guard filtering. Mixing dry and wet can produce phasing. Use MIX 0% for the undelayed dry path (with Input/Output trims), or Bypass for unchanged original samples. MIX uses a linear crossfade, smoothed over 20 ms, and applies to every active mode.

Filter endpoints are limited to 45% of the sample rate when necessary. Filters use 12 dB/octave slopes. CUTOFF and RESONANCE are inactive in Volume mode. Filter modes sweep frequency; they do not also apply the volume attack.

## What happens between picks?

A sharp enough rise in input level starts a new sweep, even if the previous one hasn’t finished. Once it reaches the end, it stays there until the next trigger. This includes OUT mode when volume has reached silence: detection always uses input before the effect. Holding a note or turning it down doesn’t restart the curve. The detector listens for rises in level, so there’s no need to get back to silence between notes.

The stereo-linked detector compares a fast peak envelope with a 20 ms moving baseline. A 25 ms peak hold suppresses waveform-cycle triggers, and the adjustable Retrigger Time avoids multiple triggers from one pick. Hysteresis rearms when the relative rise settles; the input can remain loud. Switching or adjusting enabled trigger filters briefly inhibits triggers for 50 ms while the detector settles; cutoff changes are smoothed over 20 ms. A fixed −80 dBFS reference floor avoids treating numerical silence as a meaningful transient. The first qualifying sound on activation can trigger a sweep as the detector establishes its baseline. Heavy compression can flatten pick attacks. Try raising Sensitivity; if the attack is completely flattened, the detector may have nothing left to catch.

Retriggers return the control envelope to its start over 2 ms to soften gain and filter jumps, then run the full ATTACK duration. This transition does not buffer audio. There is no lookahead, so a little of a pick can pass before the detector responds and the reset completes. Very long attacks may outlast an uncompressed guitar note's natural sustain.

## Knobs and saved settings

Double-click a knob to put it back to its default. FROM and TO reset to 0 Hz, MIX to 100%, INPUT/OUTPUT to 0 dB, ATTACK to 500 ms, SENSITIVITY to 50%, CUTOFF to 8000 Hz, RESONANCE to 0.707 Q, RETRIGGER TIME to 120 ms, and trigger filters to 120/8000 Hz. Double-clicking a numeric text field enters text editing instead. Drag dials vertically or horizontally; click the numeric field to type units such as `2 s`, `250 ms`, or `4 kHz`. You can automate all fifteen controls in Logic, and they save with your session. Older sessions keep their settings and get the defaults for any new controls, including MIX at 100%. Changes to gain, cutoff, resonance, direction, shift endpoints, Mix, and active mode are smoothed over 20 ms. Bypass takes effect straight away and skips all the controls. Switching it while audio is playing can click.

The plugin reports **0 samples** latency in every mode. Your audio interface and Logic’s buffer still have their usual latency. Watch the level when you turn up Resonance: the output meter goes coral above 0 dBFS, and there’s no automatic limiter or gain adjustment.
