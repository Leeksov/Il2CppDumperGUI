# Il2CppDumperGUI for macOS

![Platform](https://img.shields.io/badge/platform-macOS%2013.5+-lightgrey)
![License](https://img.shields.io/badge/license-MIT-blue)

A graphical interface for [Il2CppDumper](https://github.com/Perfare/Il2CppDumper), designed specifically for macOS.

## 📦 Features

- 🌐 Language selection
- 📜 Script selection (for IDA, Unity, Cpp output, etc.)
- 📁 Automatic output folder detection (experimental)
- 🍎 Requires macOS 13.5 or later

## 🛠️ Requirements

- macOS **13.5+**
- Gatekeeper permissions may need to be adjusted on first launch

## 🚀 How to Use

1. Download or build `Il2CppDumperGUI` for macOS.
2. Launch the `.app` file.
3. Select the required `Game Binary` and `global-metadata.dat` files.
4. Choose your preferred language.
5. Select the desired script output format(s).
6. Click **Start** to begin dumping.

## 📂 Output

Dumped files are saved to an automatically selected folder (usually next to the input files). This behavior is currently under testing and may not work in all cases.

## ❗ Known Issues

- Automatic output folder selection is not guaranteed to work reliably.

## 📝 License

This project is licensed under the [MIT License](LICENSE).

## 🤝 Credits

- [Perfare](https://github.com/Perfare) — creator of the original Il2CppDumper
