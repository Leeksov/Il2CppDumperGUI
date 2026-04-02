import SwiftUI
import AppKit
import UniformTypeIdentifiers
import Foundation

extension Notification.Name {
    static let il2cppLog = Notification.Name("il2cppLog")
}

// ============================================================
// MARK: - Archive Extractor (IPA/APK/XAPK/ZIP support)
// ============================================================
struct ArchiveExtractor {

    struct ExtractedGame {
        var binaryPath: String
        var metadataPath: String
        var outputDir: String
        var archLabel: String // e.g. "arm64-v8a", "ARM64"
    }

    /// Extract IPA: Payload/X.app/Frameworks/UnityFramework.framework/UnityFramework OR Payload/X.app/X
    /// + Payload/X.app/Data/Managed/Metadata/global-metadata.dat
    static func extractIPA(ipaPath: String, baseOutputDir: String, log: @escaping (String) -> Void) -> [ExtractedGame] {
        let fm = FileManager.default
        let tempDir = NSTemporaryDirectory() + "Il2CppDumperIPA_\(UUID().uuidString)/"

        log("Extracting IPA: \(URL(fileURLWithPath: ipaPath).lastPathComponent)")
        guard unzip(zipPath: ipaPath, destDir: tempDir) else {
            log("ERROR: Failed to unzip IPA"); return []
        }

        let payloadDir = tempDir + "Payload/"
        guard let apps = try? fm.contentsOfDirectory(atPath: payloadDir),
              let appDir = apps.first(where: { $0.hasSuffix(".app") }) else {
            log("ERROR: No .app found in IPA"); try? fm.removeItem(atPath: tempDir); return []
        }

        let appPath = payloadDir + appDir + "/"
        let appName = String(appDir.dropLast(4))

        let metadataSrc = appPath + "Data/Managed/Metadata/global-metadata.dat"
        guard fm.fileExists(atPath: metadataSrc) else {
            log("ERROR: global-metadata.dat not found in IPA"); try? fm.removeItem(atPath: tempDir); return []
        }

        let frameworkPath = appPath + "Frameworks/UnityFramework.framework/UnityFramework"
        let appBinaryPath = appPath + appName

        let binarySrc: String
        if fm.fileExists(atPath: frameworkPath) {
            binarySrc = frameworkPath
            log("Found UnityFramework binary")
        } else if fm.fileExists(atPath: appBinaryPath) {
            binarySrc = appBinaryPath
            log("Found app binary: \(appName)")
        } else {
            log("ERROR: No IL2CPP binary found in IPA"); try? fm.removeItem(atPath: tempDir); return []
        }

        // Copy extracted files to output dir so temp can be cleaned
        let outDir = baseOutputDir.hasSuffix("/") ? baseOutputDir : baseOutputDir + "/"
        try? fm.createDirectory(atPath: outDir, withIntermediateDirectories: true)
        let binaryDst = outDir + URL(fileURLWithPath: binarySrc).lastPathComponent
        let metadataDst = outDir + "global-metadata.dat"
        try? fm.removeItem(atPath: binaryDst)
        try? fm.removeItem(atPath: metadataDst)
        do {
            try fm.copyItem(atPath: binarySrc, toPath: binaryDst)
            try fm.copyItem(atPath: metadataSrc, toPath: metadataDst)
        } catch {
            log("ERROR: Failed to copy extracted files: \(error.localizedDescription)")
            try? fm.removeItem(atPath: tempDir); return []
        }
        log("Extracted binary and metadata to output directory")

        // Clean temp
        try? fm.removeItem(atPath: tempDir)

        return [ExtractedGame(binaryPath: binaryDst, metadataPath: metadataDst, outputDir: outDir, archLabel: "ARM64")]
    }

    /// Extract APK: lib/{arch}/libil2cpp.so + assets/bin/Data/Managed/Metadata/global-metadata.dat
    static func extractAPK(apkPath: String, baseOutputDir: String, log: @escaping (String) -> Void) -> [ExtractedGame] {
        let fm = FileManager.default
        let tempDir = NSTemporaryDirectory() + "Il2CppDumperAPK_\(UUID().uuidString)/"

        log("Extracting APK: \(URL(fileURLWithPath: apkPath).lastPathComponent)")
        guard unzip(zipPath: apkPath, destDir: tempDir) else {
            log("ERROR: Failed to unzip APK"); return []
        }

        let metadataSrc = tempDir + "assets/bin/Data/Managed/Metadata/global-metadata.dat"
        guard fm.fileExists(atPath: metadataSrc) else {
            log("ERROR: global-metadata.dat not found in APK. This may not be an IL2CPP game.")
            try? fm.removeItem(atPath: tempDir); return []
        }

        var results: [ExtractedGame] = []
        let archs = ["arm64-v8a", "armeabi-v7a", "x86_64", "x86"]
        for arch in archs {
            let soSrc = tempDir + "lib/\(arch)/libil2cpp.so"
            if fm.fileExists(atPath: soSrc) {
                let outDir = baseOutputDir.hasSuffix("/") ? baseOutputDir + arch + "/" : baseOutputDir + "/" + arch + "/"
                try? fm.createDirectory(atPath: outDir, withIntermediateDirectories: true)
                let soDst = outDir + "libil2cpp.so"
                let metaDst = outDir + "global-metadata.dat"
                try? fm.removeItem(atPath: soDst); try? fm.removeItem(atPath: metaDst)
                try? fm.copyItem(atPath: soSrc, toPath: soDst)
                try? fm.copyItem(atPath: metadataSrc, toPath: metaDst)
                results.append(ExtractedGame(binaryPath: soDst, metadataPath: metaDst, outputDir: outDir, archLabel: arch))
                log("Found \(arch)")
            }
        }

        try? fm.removeItem(atPath: tempDir)

        if results.isEmpty { log("ERROR: No libil2cpp.so found in APK") }
        return results
    }

    /// Extract split APK (APKS/XAPK/ZIP containing multiple APKs)
    static func extractSplitAPK(path: String, baseOutputDir: String, log: @escaping (String) -> Void) -> [ExtractedGame] {
        let fm = FileManager.default
        let tempDir = NSTemporaryDirectory() + "Il2CppDumperSplit_\(UUID().uuidString)/"

        log("Extracting split APK: \(URL(fileURLWithPath: path).lastPathComponent)")
        guard unzip(zipPath: path, destDir: tempDir) else {
            log("ERROR: Failed to unzip"); return []
        }

        guard let entries = try? fm.contentsOfDirectory(atPath: tempDir) else {
            try? fm.removeItem(atPath: tempDir); return []
        }
        let apkFiles = entries.filter { $0.hasSuffix(".apk") }

        if apkFiles.isEmpty {
            log("No .apk files found inside archive, trying as regular APK")
            try? fm.removeItem(atPath: tempDir)
            return extractAPK(apkPath: path, baseOutputDir: baseOutputDir, log: log)
        }

        var metadataTempPath = ""

        // First pass: find metadata
        for apkFile in apkFiles {
            if apkFile.hasPrefix("config.") { continue }
            let apkTempDir = tempDir + "apk_\(apkFile)/"
            guard unzip(zipPath: tempDir + apkFile, destDir: apkTempDir) else { continue }
            let meta = apkTempDir + "assets/bin/Data/Managed/Metadata/global-metadata.dat"
            if fm.fileExists(atPath: meta) {
                log("Found global-metadata.dat in \(apkFile)")
                metadataTempPath = meta
                break
            }
        }

        guard !metadataTempPath.isEmpty else {
            log("ERROR: No global-metadata.dat found in any split APK")
            try? fm.removeItem(atPath: tempDir); return []
        }

        // Second pass: find libil2cpp.so and copy to output
        var allResults: [ExtractedGame] = []
        for apkFile in apkFiles {
            let apkTempDir = tempDir + "apk_\(apkFile)/"
            if !fm.fileExists(atPath: apkTempDir) {
                guard unzip(zipPath: tempDir + apkFile, destDir: apkTempDir) else { continue }
            }
            for arch in ["arm64-v8a", "armeabi-v7a", "x86_64", "x86"] {
                let soSrc = apkTempDir + "lib/\(arch)/libil2cpp.so"
                if fm.fileExists(atPath: soSrc) {
                    let outDir = baseOutputDir.hasSuffix("/") ? baseOutputDir + arch + "/" : baseOutputDir + "/" + arch + "/"
                    try? fm.createDirectory(atPath: outDir, withIntermediateDirectories: true)
                    let soDst = outDir + "libil2cpp.so"
                    let metaDst = outDir + "global-metadata.dat"
                    try? fm.removeItem(atPath: soDst); try? fm.removeItem(atPath: metaDst)
                    try? fm.copyItem(atPath: soSrc, toPath: soDst)
                    try? fm.copyItem(atPath: metadataTempPath, toPath: metaDst)
                    allResults.append(ExtractedGame(binaryPath: soDst, metadataPath: metaDst, outputDir: outDir, archLabel: arch))
                    log("Found \(arch)")
                }
            }
        }

        try? fm.removeItem(atPath: tempDir)
        if allResults.isEmpty { log("ERROR: No libil2cpp.so in split APKs") }
        return allResults
    }

    /// Detect file type and extract accordingly
    static func autoExtract(filePath: String, baseOutputDir: String, log: @escaping (String) -> Void) -> [ExtractedGame] {
        let ext = URL(fileURLWithPath: filePath).pathExtension.lowercased()
        switch ext {
        case "ipa":
            return extractIPA(ipaPath: filePath, baseOutputDir: baseOutputDir, log: log)
        case "apk":
            return extractAPK(apkPath: filePath, baseOutputDir: baseOutputDir, log: log)
        case "apks", "xapk", "apkm", "zip":
            return extractSplitAPK(path: filePath, baseOutputDir: baseOutputDir, log: log)
        default:
            return []
        }
    }

    static func isArchive(_ path: String) -> Bool {
        let ext = URL(fileURLWithPath: path).pathExtension.lowercased()
        return ["ipa", "apk", "apks", "xapk", "apkm", "zip"].contains(ext)
    }

    // Native Swift ZIP extraction (no Process needed, works in sandbox)
    private static func unzip(zipPath: String, destDir: String) -> Bool {
        let fm = FileManager.default
        try? fm.createDirectory(atPath: destDir, withIntermediateDirectories: true)

        // Try Process first (works when not sandboxed)
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/ditto")
        process.arguments = ["-xk", zipPath, destDir]
        process.standardOutput = FileHandle.nullDevice
        process.standardError = FileHandle.nullDevice
        do {
            try process.run()
            process.waitUntilExit()
            if process.terminationStatus == 0 {
                return true
            }
        } catch {
            // ditto failed (sandbox?), try /usr/bin/unzip
        }

        // Fallback: /usr/bin/unzip
        let unzipProc = Process()
        unzipProc.executableURL = URL(fileURLWithPath: "/usr/bin/unzip")
        unzipProc.arguments = ["-o", "-q", zipPath, "-d", destDir]
        unzipProc.standardOutput = FileHandle.nullDevice
        unzipProc.standardError = FileHandle.nullDevice
        do {
            try unzipProc.run()
            unzipProc.waitUntilExit()
            return unzipProc.terminationStatus == 0
        } catch {
            return false
        }
    }
}

// ============================================================
// MARK: - Main View
// ============================================================
struct MainView: View {
    @State private var selectedTab = 0
    @EnvironmentObject var localization: LocalizationManager

    @State private var executableFilePath = ""
    @State private var globalMetadataPath = ""
    @State private var outputDirectoryPath = ""
    @State private var codeRegistration = ""
    @State private var metadataRegistration = ""
    @State private var logText = ""
    @State private var isDumping = false
    @State private var autoSetOutputDirectory = false
    @State private var scripts = Script.availableScripts

    @State private var dumpMethod = true
    @State private var dumpField = true
    @State private var dumpProperty = false
    @State private var dumpAttribute = false
    @State private var dumpFieldOffset = true
    @State private var dumpMethodOffset = true
    @State private var dumpTypeDefIndex = true
    @State private var generateStruct = true
    @State private var forceIl2CppVersion = false
    @State private var forceVersion = "24.3"
    @State private var forceDump = false

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 0) {
                tabButton(LocalizedString("tab_main"), index: 0)
                tabButton(LocalizedString("tab_settings"), index: 1)
                tabButton(LocalizedString("tab_about"), index: 2)
                Spacer()
                Text("v3.0.0").font(.system(size: 12)).foregroundColor(.secondary).padding(.trailing, 12)
            }
            .padding(.top, 8).padding(.leading, 8)
            Divider()
            Group {
                switch selectedTab {
                case 0: mainTabContent
                case 1: settingsTabContent
                case 2: aboutTabContent
                default: mainTabContent
                }
            }
        }
        .frame(minWidth: 720, minHeight: 500)
    }

    private func tabButton(_ title: String, index: Int) -> some View {
        Button(action: { selectedTab = index }) {
            Text(title)
                .font(.system(size: 16, weight: selectedTab == index ? .bold : .regular))
                .foregroundColor(selectedTab == index ? .primary : .secondary)
                .padding(.horizontal, 16).padding(.vertical, 6)
        }
        .buttonStyle(.plain)
    }

    // MARK: - Main Tab
    private var mainTabContent: some View {
        VStack(spacing: 0) {
            VStack(spacing: 6) {
                fileRow(LocalizedString("executable_file_label"), text: $executableFilePath, bold: true) {
                    pickFile(for: $executableFilePath, extensions: [])
                }
                fileRow(LocalizedString("global_metadata_label"), text: $globalMetadataPath, bold: true) {
                    pickFile(for: $globalMetadataPath, extensions: ["dat"])
                }
                fileRow(LocalizedString("output_directory_label"), text: $outputDirectoryPath, bold: false) {
                    pickFolder(for: $outputDirectoryPath)
                }
            }
            .padding(.horizontal, 12).padding(.top, 8)

            HStack(spacing: 4) {
                Text(LocalizedString("code_registration")).font(.system(size: 11))
                TextField("", text: $codeRegistration).textFieldStyle(.roundedBorder).font(.system(size: 11, design: .monospaced)).frame(minWidth: 80)
                Text(LocalizedString("metadata_registration")).font(.system(size: 11))
                TextField("", text: $metadataRegistration).textFieldStyle(.roundedBorder).font(.system(size: 11, design: .monospaced)).frame(minWidth: 80)
            }
            .padding(.horizontal, 12).padding(.top, 6)

            HStack(spacing: 6) {
                Button(action: openOutputFolder) {
                    Text(LocalizedString("open_output_dir")).font(.system(size: 14)).frame(maxWidth: .infinity, minHeight: 36)
                }.frame(width: 150).controlSize(.large)

                Button(action: startDump) {
                    Text(isDumping ? LocalizedString("dumping_progress") : LocalizedString("start_button"))
                        .font(.system(size: 14)).frame(maxWidth: .infinity, minHeight: 36)
                }
                .controlSize(.large).buttonStyle(.borderedProminent).disabled(isDumping)
                .onDrop(of: [.fileURL], isTargeted: nil) { providers in
                    handleDrop(providers); return true
                }
            }
            .padding(.horizontal, 12).padding(.vertical, 8)

            ScrollViewReader { proxy in
                ScrollView {
                    Text(logText)
                        .font(.system(size: 12, design: .monospaced))
                        .foregroundColor(.green.opacity(0.9))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .textSelection(.enabled).padding(8).id("logEnd")
                }
                .onChange(of: logText) { _ in withAnimation { proxy.scrollTo("logEnd", anchor: .bottom) } }
            }
            .background(Color.black.opacity(0.85)).cornerRadius(4)
            .padding(.horizontal, 12).padding(.bottom, 10)
        }
    }

    // MARK: - Settings Tab
    private var settingsTabContent: some View {
        HStack(alignment: .top, spacing: 0) {
            VStack(alignment: .leading, spacing: 0) {
                settingsSection(LocalizedString("general")) {
                    Toggle(LocalizedString("auto_set_output_dir"), isOn: $autoSetOutputDirectory)
                    Picker(LocalizedString("language"), selection: $localization.currentLanguage) {
                        ForEach(Language.allCases, id: \.self) { Text($0.displayName) }
                    }
                    .onChange(of: localization.currentLanguage) { localization.setLanguage($0) }
                }
                settingsSection(LocalizedString("dump_options")) {
                    Toggle(LocalizedString("dump_methods"), isOn: $dumpMethod)
                    Toggle(LocalizedString("dump_fields"), isOn: $dumpField)
                    Toggle(LocalizedString("dump_properties"), isOn: $dumpProperty)
                    Toggle(LocalizedString("dump_attributes"), isOn: $dumpAttribute)
                    Toggle(LocalizedString("dump_field_offsets"), isOn: $dumpFieldOffset)
                    Toggle(LocalizedString("dump_method_offsets"), isOn: $dumpMethodOffset)
                    Toggle(LocalizedString("dump_typedef_index"), isOn: $dumpTypeDefIndex)
                    Toggle(LocalizedString("generate_struct"), isOn: $generateStruct)
                }
                settingsSection(LocalizedString("advanced")) {
                    Toggle(LocalizedString("force_il2cpp_version"), isOn: $forceIl2CppVersion)
                    if forceIl2CppVersion {
                        HStack { Text(LocalizedString("version_label")).font(.system(size: 12)); TextField("24.3", text: $forceVersion).textFieldStyle(.roundedBorder).frame(width: 70) }.padding(.leading, 20)
                    }
                    Toggle(LocalizedString("force_dump"), isOn: $forceDump)
                }
                Spacer()
            }.frame(maxWidth: .infinity, alignment: .leading).padding(.leading, 16)

            Divider()

            VStack(alignment: .leading, spacing: 0) {
                settingsSection(LocalizedString("auto_copy_scripts")) {
                    ForEach($scripts) { $script in
                        Toggle(isOn: $script.isEnabled) {
                            HStack(spacing: 4) {
                                Text(script.filename).font(.system(size: 12))
                                if script.resourcePath == nil {
                                    Image(systemName: "exclamationmark.triangle.fill").foregroundColor(.yellow).font(.system(size: 10))
                                }
                            }
                        }.toggleStyle(.checkbox)
                    }
                }
                Spacer()
            }.frame(maxWidth: .infinity, alignment: .leading).padding(.leading, 16)
        }.padding(.top, 8)
    }

    private func settingsSection<Content: View>(_ title: String, @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.system(size: 14, weight: .bold)).padding(.bottom, 4)
            content()
        }.padding(.bottom, 16)
    }

    // MARK: - About Tab
    private var aboutTabContent: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(LocalizedString("about_title")).font(.system(size: 18, weight: .bold))
            Text(LocalizedString("about_subtitle")).foregroundColor(.secondary)
            Divider().padding(.vertical, 4)
            Group {
                Text("AndnixSH").fontWeight(.semibold) + Text(" (Windows Il2CppDumper-GUI)")
                Link("https://github.com/AndnixSH/Il2CppDumper-GUI", destination: URL(string: "https://github.com/AndnixSH/Il2CppDumper-GUI")!).foregroundColor(.blue)
            }
            Group {
                Text("Perfare").fontWeight(.semibold) + Text(" (Il2CppDumper)")
                Link("https://github.com/Perfare/Il2CppDumper", destination: URL(string: "https://github.com/Perfare/Il2CppDumper")!).foregroundColor(.blue)
            }
            Group {
                Text("djkaty").fontWeight(.semibold) + Text(" (Il2CppInspector)")
                Link("https://github.com/djkaty/Il2CppInspector", destination: URL(string: "https://github.com/djkaty/Il2CppInspector")!).foregroundColor(.blue)
            }
            Divider().padding(.vertical, 4)
            Text(LocalizedString("source_code")).fontWeight(.semibold)
            Link("https://github.com/Leeksov/Il2CppDumperGUI", destination: URL(string: "https://github.com/Leeksov/Il2CppDumperGUI")!).foregroundColor(.blue)
            Spacer()
        }.padding().frame(maxWidth: .infinity, alignment: .leading)
    }

    // MARK: - Drag & Drop
    private func handleDrop(_ providers: [NSItemProvider]) {
        for provider in providers {
            provider.loadItem(forTypeIdentifier: "public.file-url", options: nil) { item, _ in
                guard let data = item as? Data,
                      let url = URL(dataRepresentation: data, relativeTo: nil) else { return }
                let path = url.path
                let ext = url.pathExtension.lowercased()
                DispatchQueue.main.async {
                    if ArchiveExtractor.isArchive(path) {
                        // IPA/APK/XAPK dropped — auto-extract and dump
                        appendLog("Dropped archive: \(url.lastPathComponent)")
                        let outBase = outputDirectoryPath.isEmpty
                            ? url.deletingLastPathComponent().appendingPathComponent(url.deletingPathExtension().lastPathComponent + "_dumped").path
                            : outputDirectoryPath
                        outputDirectoryPath = outBase
                        autoExtractAndDump(archivePath: path, outputBase: outBase)
                    } else if ext == "dat" {
                        globalMetadataPath = path
                        appendLog("Dropped metadata: \(url.lastPathComponent)")
                    } else {
                        executableFilePath = path
                        if autoSetOutputDirectory { outputDirectoryPath = url.deletingLastPathComponent().path }
                        appendLog("Dropped binary: \(url.lastPathComponent)")
                    }
                }
            }
        }
    }

    // MARK: - Helpers
    private func fileRow(_ label: String, text: Binding<String>, bold: Bool, action: @escaping () -> Void) -> some View {
        HStack(spacing: 6) {
            Text(label)
                .font(.system(size: 13, weight: bold ? .bold : .regular))
                .frame(width: 150, alignment: .leading)
            TextField("", text: text)
                .textFieldStyle(.roundedBorder)
                .font(.system(size: 12))
            Button(LocalizedString("select"), action: action)
                .frame(width: 80)
        }
    }

    // MARK: - File Pickers
    private func pickFile(for path: Binding<String>, extensions: [String]) {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true; panel.canChooseDirectories = false; panel.allowsMultipleSelection = false
        if !extensions.isEmpty { panel.allowedContentTypes = extensions.compactMap { UTType(filenameExtension: $0) } }
        if panel.runModal() == .OK, let url = panel.url {
            path.wrappedValue = url.path
            if autoSetOutputDirectory { outputDirectoryPath = url.deletingLastPathComponent().path }
        }
    }

    private func pickFolder(for path: Binding<String>) {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false; panel.canChooseDirectories = true; panel.allowsMultipleSelection = false
        if panel.runModal() == .OK, let url = panel.url { path.wrappedValue = url.path }
    }

    private func openOutputFolder() {
        guard !outputDirectoryPath.isEmpty else { return }
        NSWorkspace.shared.open(URL(fileURLWithPath: outputDirectoryPath))
    }

    // MARK: - Auto Extract & Dump (IPA/APK)
    private func autoExtractAndDump(archivePath: String, outputBase: String) {
        isDumping = true
        DispatchQueue.global(qos: .userInitiated).async {
            let games = ArchiveExtractor.autoExtract(filePath: archivePath, baseOutputDir: outputBase) { msg in
                DispatchQueue.main.async { appendLog(msg) }
            }

            if games.isEmpty {
                DispatchQueue.main.async { appendLog("ERROR: Could not extract any IL2CPP files from archive"); isDumping = false }
                return
            }

            for game in games {
                DispatchQueue.main.async {
                    appendLog("> Dumping \(game.archLabel)")
                    executableFilePath = game.binaryPath
                    globalMetadataPath = game.metadataPath
                    outputDirectoryPath = game.outputDir
                }

                try? FileManager.default.createDirectory(atPath: game.outputDir, withIntermediateDirectories: true)
                performDump(exe: game.binaryPath, meta: game.metadataPath, outDir: game.outputDir)
            }

            DispatchQueue.main.async { isDumping = false }
        }
    }

    // MARK: - Start Dump (manual or from fields)
    private func startDump() {
        // Check if executable is an archive
        if ArchiveExtractor.isArchive(executableFilePath) {
            let outBase = outputDirectoryPath.isEmpty
                ? URL(fileURLWithPath: executableFilePath).deletingLastPathComponent()
                    .appendingPathComponent(URL(fileURLWithPath: executableFilePath).deletingPathExtension().lastPathComponent + "_dumped").path
                : outputDirectoryPath
            outputDirectoryPath = outBase
            autoExtractAndDump(archivePath: executableFilePath, outputBase: outBase)
            return
        }

        guard !executableFilePath.isEmpty else { appendLog(LocalizedString("error_exe_not_specified")); return }
        guard !globalMetadataPath.isEmpty else { appendLog(LocalizedString("error_meta_not_specified")); return }
        guard !outputDirectoryPath.isEmpty else { appendLog(LocalizedString("error_output_not_specified")); return }

        isDumping = true
        appendLog(LocalizedString("starting_dump"))

        let exe = executableFilePath, meta = globalMetadataPath, outDir = outputDirectoryPath

        DispatchQueue.global(qos: .userInitiated).async {
            performDump(exe: exe, meta: meta, outDir: outDir)
            DispatchQueue.main.async {
                isDumping = false
                copySelectedScripts()
            }
        }
    }

    // MARK: - Core Dump
    private func performDump(exe: String, meta: String, outDir: String) {
        let cfgMethod = dumpMethod ? Int32(1) : 0
        let cfgField = dumpField ? Int32(1) : 0
        let cfgProp = dumpProperty ? Int32(1) : 0
        let cfgAttr = dumpAttribute ? Int32(1) : 0
        let cfgFieldOff = dumpFieldOffset ? Int32(1) : 0
        let cfgMethodOff = dumpMethodOffset ? Int32(1) : 0
        let cfgTypeIdx = dumpTypeDefIndex ? Int32(1) : 0
        let cfgForceVer = forceIl2CppVersion ? Int32(1) : 0
        let cfgVer = Double(forceVersion) ?? 24.3
        let cfgForceDump = forceDump ? Int32(1) : 0
        let manualCode = codeRegistration
        let manualMeta = metadataRegistration
        let postProcessPath = Bundle.main.path(forResource: "Il2CppPostProcess", ofType: nil) ?? ""

        let logObserver = NotificationCenter.default.addObserver(
            forName: .il2cppLog, object: nil, queue: .main
        ) { n in
            if let msg = n.userInfo?["message"] as? String { logText += "\(msg)\n" }
        }

        let result = il2cpp_dump_ex(
            exe, meta, outDir,
            cfgMethod, cfgField, cfgProp, cfgAttr,
            cfgFieldOff, cfgMethodOff, cfgTypeIdx,
            cfgForceVer, cfgVer, cfgForceDump,
            manualCode, manualMeta, postProcessPath,
            { cStr, _ in
                guard let cStr = cStr else { return }
                NotificationCenter.default.post(name: .il2cppLog, object: nil, userInfo: ["message": String(cString: cStr)])
            },
            nil
        )

        NotificationCenter.default.removeObserver(logObserver)

        DispatchQueue.main.async {
            if result == 0 {
                appendLog(LocalizedString("dump_done"))
            } else {
                let err = String(cString: il2cpp_get_last_error())
                appendLog(LocalizedString("dump_failed_code", result))
                if !err.isEmpty { appendLog(err) }
            }
        }
    }

    private func copySelectedScripts() {
        let fm = FileManager.default
        let outURL = URL(fileURLWithPath: outputDirectoryPath)
        for script in scripts where script.isEnabled && script.resourcePath != nil {
            do {
                let src = URL(fileURLWithPath: script.resourcePath!)
                let dst = outURL.appendingPathComponent(script.filename)
                if fm.fileExists(atPath: dst.path) { try fm.removeItem(at: dst) }
                try fm.copyItem(at: src, to: dst)
                appendLog(LocalizedString("copied_script", script.filename))
            } catch {
                appendLog(LocalizedString("failed_copy_script", script.filename, error.localizedDescription))
            }
        }
    }

    private func appendLog(_ text: String) {
        logText += "\(text)\n"
    }
}
