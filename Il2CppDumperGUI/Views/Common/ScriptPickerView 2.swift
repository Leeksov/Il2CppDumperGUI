//
//  ScriptPickerView 2.swift
//  Il2CppDumperGUI
//
//  Created by Leeksov on 10.5.2025.
//


import SwiftUI

struct ScriptPickerView: View {
    @Binding var scripts: [Script]
    @Binding var selectedScript: Script?  // Добавляем binding для выбранного скрипта
    
    private var sortedScripts: [Binding<Script>] {
        $scripts.sorted { $0.wrappedValue.filename.localizedCaseInsensitiveCompare($1.wrappedValue.filename) == .orderedAscending }
    }
    
    var body: some View {
        List(selection: $selectedScript) {  // Добавляем selection для List
            ForEach(sortedScripts) { $script in
                scriptRow(for: $script)
                    .padding(.vertical, 6)
                    .animation(.easeInOut(duration: 0.2), value: script.isEnabled)
                    .tag(script.wrappedValue)  // Добавляем tag для выбора
            }
        }
        .listStyle(.plain)
    }
    
    // Остальной код остается без изменений
    private func scriptRow(for script: Binding<Script>) -> some View {
        HStack(spacing: 12) {
            Toggle("", isOn: script.isEnabled)
                .toggleStyle(SwitchToggleStyle(tint: .blue))
                .labelsHidden()
            VStack(alignment: .leading, spacing: 2) {
                Text(script.wrappedValue.filename)
                    .font(.system(.body, design: .monospaced))
                    .lineLimit(1)
                
                Text(script.wrappedValue.description)
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .lineLimit(2)
            }
            
            Spacer()
            scriptStatusIcon(for: script.wrappedValue)
        }
        .contentShape(Rectangle())
    }
    
    @ViewBuilder
    private func scriptStatusIcon(for script: Script) -> some View {
        if script.resourcePath == nil {
            Image(systemName: "exclamationmark.triangle.fill")
                .foregroundColor(.yellow)
                .help(LocalizedString("script_missing"))
        } else {
            Image(systemName: "checkmark.circle.fill")
                .foregroundColor(script.isEnabled ? .green : .gray.opacity(0.5))
        }
    }
}