# mIDE

mIDE is a Windows desktop IDE for writing C-like source code and compiling it into MLOG (Mindustry Logic) programs. It is designed as a nearly feature-complete C-to-MLOG compiler with a custom editor, compiler pipeline, linting, optimization, and debugging integration.

## Features

- C-to-MLOG compilation pipeline
- Split-pane editor layout with source and generated output
- Dark mode UI and custom window chrome
- Compiler optimization controls
- Linting and warnings support
- Debug connection to a Mindustry daemon or local server
- Example C source for compiler stress testing
- C-like scalar types, qualifiers, constants, and boolean literals

## Project structure

- `mIDE/` — main application source code, editor UI, compiler logic, and resources
- `mIDE.slnx` — Visual Studio solution file
- `LICENSE` — repository license
- `mIDE/CompilerSample.c` — sample program demonstrating supported constructs

## Building

### From source

mIDE is a Windows desktop application and is built using Win32 APIs and Visual Studio project files.

1. Open `mIDE.slnx` in Visual Studio 2022, or open `mIDE/mIDE.vcxproj` directly.
2. Restore/build the solution in Debug or Release mode.
3. Run the generated `mIDE.exe`.

### Quick download

For users who want a ready-to-run executable without building from source, nightly builds are available at:

https://nightly.link/Abdullah-Ajeebi/mIDE/workflows/msbuild/main

Download the latest artifact and extract `mIDE.exe` to run directly.

## Usage

- Write or open a C source file in the editor.
- Compile it from the application menu to generate MLOG output.
- Review the generated output in the compiled panel.
- Configure debugging and compiler options in the settings dialog.
- Use `mIDE/CompilerSample.c` as a reference example for supported constructs.
- See `PLANS.md` for supported and deferred C keywords.

## Notes

This repository is focused on the Windows IDE and compiler experience, and it is primarily intended for use with Mindustry scripting workflows. The compiler targets a restricted subset of C that maps well to MLOG; it is not a general-purpose C compiler.

## License

See the `LICENSE` file for licensing details.
