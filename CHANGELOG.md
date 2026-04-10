# Changelog

All notable accessibility-focused changes in this repository should be documented here.

## V2_0

Current documented development step for the talking ARDF branch with spectrum work.

### GitHub Release Status

- documentation is prepared
- a dedicated public GitHub firmware asset may still be pending
- local and experimental builds can exist before a release asset is published

### Added

- classic display spectrum finder support
- first blind-friendly audio spectrum finder concept
- dual documentation structure for `V1` and `V2_0`
- clearer explanation of how the two variants differ

### Behavior

- `V2_0` keeps the same talking ARDF base as `V1`
- `F + 5` is used for the classic display spectrum finder
- a held `5` action is planned and documented for the audio spectrum finder workflow

### Notes

- `V2_0` is meant as an experimental extension, not as a replacement for the simpler blind-first `V1`
- the audio spectrum helper is still an early step toward a stronger blind-accessible spectrum analyser

## V1

Baseline talking ARDF direction for the simplified blind-first receiver.

### Included

- receive-only ARDF focus
- simplified controls
- spoken status prompts
- Morse fallback for items without available voice clips
- reduced menu structure for field use
