# Native engine

The C++ engine parses IL2CPP metadata and executables, then writes `dump.cs`,
`script.json`, `stringliteral.json`, `il2cpp.h`, and `DummyDll/`. CMake builds it
as a static library that Xcode links into the macOS GUI executable.
