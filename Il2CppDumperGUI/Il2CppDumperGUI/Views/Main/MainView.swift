import SwiftUI
import AppKit

struct MainView: View {
    @State private var executableFilePath = ""
    @State private var globalMetadataPath = ""
    @State private var outputDirectoryPath = ""
    @State private var logText = Constants.defaultLogText
    @State private var showSettings = false
    @State private var autoSetOutputDirectory = false
    @State private var scrollToBottomID = UUID()
    @State private var scripts = Script.availableScripts
    @EnvironmentObject var localization: LocalizationManager
    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            FilePickerView(
                titleKey: "executable_file",
                path: $executableFilePath,
                allowedFileExtensions: [],
                onFileSelected: handleFileSelection
            )
            FilePickerView(
                titleKey: "global_metadata_file",
                path: $globalMetadataPath,
                allowedFileExtensions: Constants.outputFileExtensions
            )
            FolderPickerView(titleKey: "output_folder", path: $outputDirectoryPath)
            HStack {
                Spacer()
                Button(LocalizedString("start_dumping"), action: runIl2CppDumper)
                    .buttonStyle(ActionButtonStyle())
                Spacer()
                Button(action: { showSettings = true }) {
                    Image(systemName: "gear")
                        .font(.title)
                }
                .buttonStyle(PlainButtonStyle())
            }
            
            LogView(logText: $logText, scrollToBottomID: $scrollToBottomID)
        }
        .padding()
        .frame(minWidth: 600, minHeight: 400)
        .sheet(isPresented: $showSettings) {
            SettingsView(
                showSettings: $showSettings,
                autoSetOutputDirectory: $autoSetOutputDirectory,
                scripts: $scripts
            )
            .environmentObject(localization)
        }
    }
    private func handleFileSelection(_ path: String) {
        if autoSetOutputDirectory {
            setOutputDirectoryAutomatically()
        }
    }
    private func setOutputDirectoryAutomatically() {
        guard !executableFilePath.isEmpty else {
            logText += "\n\(LocalizedString("error")): \(LocalizedString("executable_file_not_specified"))"
            return
        }
        let directoryURL = URL(fileURLWithPath: executableFilePath).deletingLastPathComponent()
        outputDirectoryPath = directoryURL.path
    }
    private func runIl2CppDumper() {
        DispatchQueue.global(qos: .userInitiated).async {
            guard let il2cppDumperPath = Bundle.main.path(
                forResource: Constants.il2cppDumperName,
                ofType: nil
            ) else {
                logError(LocalizedString("il2cpp_not_found"))
                return
            }
            
            guard !executableFilePath.isEmpty else {
                logError(LocalizedString("executable_file_not_specified"))
                return
            }
            
            guard !globalMetadataPath.isEmpty else {
                logError(LocalizedString("metadata_not_specified"))
                return
            }
            
            guard !outputDirectoryPath.isEmpty else {
                logError(LocalizedString("output_not_specified"))
                return
            }
            
            let task = Process()
            task.launchPath = il2cppDumperPath
            task.arguments = [executableFilePath, globalMetadataPath, outputDirectoryPath]
            let pipe = Pipe()
            task.standardOutput = pipe
            task.standardError = pipe
            pipe.fileHandleForReading.readabilityHandler = { handle in
                let data = handle.availableData
                if let output = String(data: data, encoding: .utf8) {
                    DispatchQueue.main.async {
                        processOutput(output, task: task, handle: handle)
                    }
                }
            }
            do {
                try task.run()
                task.waitUntilExit()
                DispatchQueue.main.async {
                    handleProcessCompletion(task: task)
                }
            } catch {
                logError("\(LocalizedString("failed_to_launch")): \(error.localizedDescription)")
            }
        }
    }
    private func processOutput(_ output: String, task: Process, handle: FileHandle) {
        guard !output.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return }
        if output.contains("Press any key to exit...") {
            task.terminate()
            handle.readabilityHandler = nil
            logText += "\n" + LocalizedString("dump_completed")
            copySelectedScripts()
        } else {
            logText += "\n\(output)"
        }
    }
    private func handleProcessCompletion(task: Process) {
        let status = task.terminationStatus
        logText += status == 0 ?
            "\n" + LocalizedString("process_finished") :
            "\n" + LocalizedString("process_failed", status)
    }
    private func copySelectedScripts() {
        let fileManager = FileManager.default
        let outputURL = URL(fileURLWithPath: outputDirectoryPath)
        
        let scriptsToCopy = scripts.filter { $0.isEnabled && $0.resourcePath != nil }
        
        guard !scriptsToCopy.isEmpty else {
            logText += "\n" + LocalizedString("no_scripts_selected")
            return
        }
        DispatchQueue.global(qos: .utility).async {
            var successCount = 0
            var errorMessages = [String]()
            
            for script in scriptsToCopy {
                do {
                    let sourceURL = URL(fileURLWithPath: script.resourcePath!)
                    let destinationURL = outputURL.appendingPathComponent(script.filename)
                    
                    if fileManager.fileExists(atPath: destinationURL.path) {
                        try fileManager.removeItem(at: destinationURL)
                    }
                    
                    try fileManager.copyItem(at: sourceURL, to: destinationURL)
                    successCount += 1
                } catch {
                    errorMessages.append("\(script.filename): \(error.localizedDescription)")
                }
            }
            DispatchQueue.main.async {
                var resultMessage = "\n" + LocalizedString("scripts_report")
                resultMessage += "\n" + LocalizedString("successfully_copied", successCount, scriptsToCopy.count)
                if !errorMessages.isEmpty {
                    resultMessage += "\n\(LocalizedString("errors")):\n     - " + errorMessages.joined(separator: "\n     - ")
                }
                if successCount > 0 {
                    resultMessage += "\n" + LocalizedString("scripts_available")
                }
                logText += resultMessage
            }
        }
    }
    private func logError(_ message: String) {
            DispatchQueue.main.async {
                logText += "\n\(LocalizedString("error")): \(message)"
            }
        }
    }
