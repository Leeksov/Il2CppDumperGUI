import SwiftUI

struct SettingsView: View {
    @Binding var showSettings: Bool
    @Binding var autoSetOutputDirectory: Bool
    @Binding var scripts: [Script]
    @EnvironmentObject var localization: LocalizationManager
    
    var body: some View {
        VStack(spacing: 20) {
            HStack {
                Text(LocalizedString("settings_title"))
                    .font(.system(size: 13, weight: .semibold))
                Spacer()
            }
            .padding([.top, .horizontal])
            Form {
                Section {
                    Toggle(LocalizedString("auto_output_dir"), isOn: $autoSetOutputDirectory)
                    
                    Picker(LocalizedString("app_language"), selection: $localization.currentLanguage) {
                        ForEach(Language.allCases, id: \.self) { language in
                            Text(language.displayName)
                        }
                    }
                    .onChange(of: localization.currentLanguage) { newValue in
                        localization.setLanguage(newValue)
                    }
                }
                Section(header: Text(LocalizedString("script_selection")).padding(.top)) {
                    ScriptSelectionView(scripts: $scripts)
                        .padding(.vertical, 8)
                }
            }
            .formStyle(.grouped)
            HStack {
                Spacer()
                Button(LocalizedString("done_button")) {
                    showSettings = false
                }
                .keyboardShortcut(.defaultAction)
            }
            .padding()
        }
        .frame(width: 500, height: 400)
        .id(localization.currentLanguage)
    }
}
struct ScriptSelectionView: View {
    @Binding var scripts: [Script]
    @State private var isMenuPresented = false
    @EnvironmentObject var localization: LocalizationManager
    
    private var sortedScripts: [Script] {
        scripts.sorted { $0.filename.localizedCaseInsensitiveCompare($1.filename) == .orderedAscending }
    }
    var selectedScriptsText: String {
        let selectedCount = scripts.filter { $0.isEnabled }.count
        return LocalizedString("selected_count", selectedCount, scripts.count)
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Button(action: { isMenuPresented.toggle() }) {
                HStack {
                    Text(selectedScriptsText)
                    Spacer()
                    Image(systemName: "chevron.down")
                }
                .padding(8)
                .background(Color(.controlBackgroundColor))
                .cornerRadius(4)
                .overlay(
                    RoundedRectangle(cornerRadius: 4)
                        .stroke(Color(.separatorColor), lineWidth: 1)
                )
            }
            .buttonStyle(.plain)
            .popover(isPresented: $isMenuPresented) {
                VStack(alignment: .leading) {
                    ForEach(sortedScripts) { script in
                        Toggle(isOn: Binding(
                            get: { script.isEnabled },
                            set: { updateScript(script, $0) }
                        )) {
                            HStack {
                                Text(script.filename)
                                if script.resourcePath == nil {
                                    Image(systemName: "exclamationmark.triangle.fill")
                                        .foregroundColor(.yellow)
                                }
                            }
                        }
                        .toggleStyle(.checkbox)
                        .padding(.vertical, 4)
                    }
                }
                .padding()
                .frame(width: 300)
            }
        }
    }
    private func updateScript(_ script: Script, _ value: Bool) {
        if let index = scripts.firstIndex(where: { $0.id == script.id }) {
            scripts[index].isEnabled = value
        }
    }
}
