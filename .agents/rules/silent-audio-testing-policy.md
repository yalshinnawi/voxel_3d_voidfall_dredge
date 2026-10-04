# STRICT NON-AUDIBLE & SILENT TESTING POLICY (DEVELOPER EAR PROTECTION)

## CRITICAL RULE: ZERO AUDIBLE AUDIO DURING TESTS & AUTOMATION
Under NO circumstance should any test suite, automated playthrough, visual harness, or background test command output audible sound to the user's physical speakers or headset.

### Invariants for All Agents & Future Development
1. **Never Enable Hardware Audio in Tests**:
   - All unit, integration, and E2E test executables (including `test_audio.exe`, `test_enemy_stalker.exe`, `test_unit_all.exe`, etc.) MUST run 100% silently in headless audio mode.
   - All audio DSP, ear-safety limiters, and voice mixing are evaluated mathematically in CPU RAM buffers (`render_mix()`, `render_offline_samples()`). Physical audio devices (`waveOutOpen`) must NEVER be initialized during testing.

2. **Automated Visual Runs & Captures Must Be Silent**:
   - Whenever executing `VoidfallDredge.exe` with automated flags (`--test-enemy`, `--auto-play-test`, `--capture-models`, `--capture-level-shapes`, `--hidden`, `--screenshot`, or `--test`), ALWAYS ensure the process runs silently.
   - Pass `--mute` or `--silent` whenever launching the game executable from automated scripts or subagents.
   - The engine automatically suppresses hardware audio if any `--test*`, `--capture*`, or `--mute` flag is detected on the command line.

3. **Opt-in Audio Only**:
   - The `--audible` flag is strictly reserved for when a human developer explicitly requests to listen to audio output. Agents must never add `--audible` to automated scripts or testing commands.

4. **Environment Safety Net**:
   - `export VOIDFALL_MUTE_AUDIO=1` unconditionally forces the audio engine into silent headless mode across all executables.
