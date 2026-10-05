# interactor-cineform

A headless wavelet video encoder that takes jobs over the shared-memory bus and writes Matroska files.

## What it is for

A job arrives as a command on the bus. The encoder opens the source the job names, encodes its frames and any interleaved audio, and answers with a progress sample per frame and one verdict at the end; pixels never cross the bus. It is the encoder in the pair RFD 1137 describes, with `transport-cineform-tui` sending jobs and `service-cineform` hosting both.

## Build

Outside the workspace, the dependencies come from this repository's own `default.xml`:

```sh
repo init -u https://github.com/V-Sekai-fire/interactor-cineform -m default.xml
repo sync
cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build
```

The policy minimum is there because the pinned codec SDK asks for an older CMake. Inside the workspace checkout, build it through `service-cineform` instead of running `repo init` here.

## Licence

Apache-2.0 OR MIT; see LICENSE-APACHE and LICENSE-MIT.
