# L8BLOOMER manual

Start with **Volume**, **Direction IN**, **Attack 500 ms**, and **Sensitivity 50%**. Each accepted pick restarts the sweep, including over amp noise or a previous note's sustain. Silence is not required. Increase sensitivity for smaller changes in volume; decrease it if unwanted fluctuations trigger the effect. LISTENING means the detector can accept a new onset; WAIT indicates that it is locked out, settling, or waiting for the previous rise to settle. Place delays and reverbs after L8BLOOMER if you want their tails to continue between swells.

## Controls

| Control | Range / behavior |
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

Advanced can be collapsed without changing its settings. Try Retrigger Time around 150–250 ms if one pick produces several triggers; shorten it for faster playing. Picks inside the interval are discarded, not queued. Changing the interval applies to elapsed time since the most recent accepted trigger. At 0 ms, there is no time lockout, but the detector still uses onset hysteresis.

Enable Trigger Filter to focus detection on a frequency band. Low-Cut removes rumble; raise it toward 1000 Hz to favour pick noise. High-Cut can exclude hiss above the useful band. These are 24 dB/octave Butterworth filters, not brick-wall frequency gates, and are applied only to the detector copy of the input. They do not EQ the audible signal. OFF skips their sample processing and coefficient updates entirely. With crossed cutoffs (Low-Cut at or above High-Cut), detection is disabled until a valid band is restored. Bounds are limited to 45% of the sample rate internally.

| Mode | Direction IN | Direction OUT |
| --- | --- | --- |
| Bypass | Original samples, unchanged | Original samples, unchanged |
| Volume | Silence → original input amplitude | Original input amplitude → silence |
| High-pass | 20 kHz → CUTOFF | CUTOFF → 20 kHz |
| Low-pass | 20 Hz → CUTOFF | CUTOFF → 20 Hz |
| Frequency shift | FROM → TO | TO → FROM |

Frequency Shift adds a number of Hz to every input frequency, creating dissonant/metallic sounds. It does not shift by a musical interval. FROM is the left endpoint knob (CUTOFF's position) and TO is the right endpoint knob (RESONANCE's position). Existing sessions retain their sweep: the former BOTTOM value is FROM and the former TOP value is TO. Their values are saved separately from the filter controls, so switching modes preserves each set. They are named endpoints, not sorted limits: either may be positive, negative, or zero. The bipolar knob scales are symmetric, with finer resolution near zero and integer-Hz values. Try FROM 0 Hz, TO +500 Hz, ATTACK 500 ms, and MIX 50%.

The shifter uses an original, optimised 20-section IIR allpass pair and quadrature modulation. The oscillator keeps running through retriggers so its phase does not jump. A 12th-order low-pass guard reduces high-frequency aliasing for upward shifts; its cutoff follows the largest positive endpoint and may soften the extreme high end. At unusually low sample rates, the effective shift is capped at one quarter of the sample rate. Negative shifts that cross zero fold into positive output frequencies, as expected for real-valued frequency shifting.

There is no FFT/block delay or lookahead, but the IIR filters have frequency-dependent phase/group delay. Zero Hz means no frequency translation, not bit-identical bypass; the wet path retains filter phase and high-frequency guard filtering. Mixing dry and wet can produce phasing. Use MIX 0% for the undelayed dry path (with Input/Output trims), or Bypass for unchanged original samples. MIX uses a linear crossfade, smoothed over 20 ms, and applies to every active mode.

Filter endpoints are limited to 45% of the sample rate when necessary. Filters use 12 dB/octave slopes. CUTOFF and RESONANCE are inactive in Volume mode. Filter modes sweep frequency; they do not also apply the volume attack.

Each detected positive level transient starts one sweep, interrupting any sweep already running. The curve then holds its endpoint until the next transient. This includes OUT mode when volume has reached silence: detection always uses input before the effect. Merely continuing a note or turning its volume down does not restart the curve. This is an onset detector, not a noise gate, and it does not require silence or close automatically between notes.

The stereo-linked detector compares a fast peak envelope with a 20 ms moving baseline. A 25 ms peak hold suppresses waveform-cycle triggers, and the adjustable Retrigger Time avoids multiple triggers from one pick. Hysteresis rearms when the relative rise settles; the input can remain loud. Switching or adjusting enabled trigger filters briefly inhibits triggers for 50 ms while the detector settles; cutoff changes are smoothed over 20 ms. A fixed −80 dBFS reference floor avoids treating numerical silence as a meaningful transient. The first qualifying sound on activation can trigger a sweep as the detector establishes its baseline. Very heavy compression can flatten pick dynamics enough to require higher sensitivity or prevent detection altogether.

Retriggers return the control envelope to its start over 2 ms to soften gain and filter jumps, then run the full ATTACK duration. This transition does not buffer audio. There is no lookahead, so a little of a pick can pass before the detector responds and the reset completes. Very long attacks may outlast an uncompressed guitar note's natural sustain.

Double-click any knob's dial to reset its currently assigned parameter to its default. FROM and TO reset to 0 Hz, MIX to 100%, INPUT/OUTPUT to 0 dB, ATTACK to 500 ms, SENSITIVITY to 50%, CUTOFF to 8000 Hz, RESONANCE to 0.707 Q, RETRIGGER TIME to 120 ms, and trigger filters to 120/8000 Hz. Double-clicking a numeric text field enters text editing instead. Drag dials vertically or horizontally; click the numeric field to type units such as `2 s`, `250 ms`, or `4 kHz`. All fifteen parameters support Logic automation and session recall. Older sessions keep their existing settings and receive defaults for new controls (including MIX 100%). Changes to gain, cutoff, resonance, direction, shift endpoints, Mix, and active mode are smoothed over 20 ms. Bypass takes effect immediately and ignores all controls; toggling hard bypass at a nonzero sample can create a discontinuity.

The plugin reports **0 samples** latency in every mode. Audio interface and Logic buffer latency still apply. High resonance can boost levels; the output meter turns coral above 0 dBFS. There is no hidden limiter or automatic gain compensation.
