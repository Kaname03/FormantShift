# VoiceModeler (JUCE VST3 Example)

Minimal JUCE-based VST3 plugin built with CMake.
Pushing to GitHub triggers GitHub Actions to build VST3 on Windows and macOS and uploads artifacts.
Tagging a commit creates a Release with binaries.

## Build locally
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
