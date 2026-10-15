# FL Studio integration — development branch v0.1
## Tape movement in FL Studio
Create three native FL Automation Clips targeting TapeDrive parameters: WOW, FLUTTER, TAPE AGE (each is a VST3 host parameter).
Suggested safe starting 8-bar envelopes (normalized 0..1, artistic presets, not captured device calibration):
- Fresh tape: WOW .05, FLUTTER .04, AGE .02.
- Aging: WOW .10->.28, FLUTTER .08->.32, AGE .15->.60.
- Breakdown: WOW .25->.60, FLUTTER .30->.65, AGE .50->.90.
Prefer smooth interpolation, avoid automated bypass toggles and feedback loops.
## Regression / optimization
Already has VST3 sample-position automation tests. Measure CPU/allocations with all artefacts on 44.1/48/96 kHz; compare audio null when automation is constant against v1.2.
## Current branch
Integration prescription only, no DSP/GUI/preset-state changes.