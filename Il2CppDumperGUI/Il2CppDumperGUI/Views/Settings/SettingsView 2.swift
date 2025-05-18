//
//  SettingsView 2.swift
//  Il2CppDumperGUI
//
//  Created by Leeksov on 10.5.2025.
//


import SwiftUI

struct SettingsView: View {
    @Binding var showSettings: Bool
    @Binding var autoSetOutputDirectory: Bool
    @Binding var scripts: [Script]
    @State private var selectedLanguage: Language = .english
    @State private var selectedScript: Script?  // Добавляем состояние для выбранного скрипта
    
    var enabledScriptsCount: Int {
        scripts.filter { $0.isEnabled }.count
    }
    
    var body: some View {
        TabView {
            // Первая вкладка (без изменений)
            VStack(spacing: 20) {
                Text(LocalizedString("general_settings"))
                    .font(.title)
                
                Toggle(LocalizedString("auto_set_output"), isOn: $autoSetOutputDirectory)
                    .toggleStyle(SwitchToggleStyle(tint: .blue))
                
                Picker("Language", selection: $selectedLanguage) {
                    ForEach(Language.allCases, id: \.self) { language in
                        Text(language.displayName).tag(language)
                    }
                }
                .pickerStyle(SegmentedPickerStyle())
                .padding()
                .onChange(of: selectedLanguage) { newValue in
                    LocalizationManager.shared.setLanguage(newValue)
                }
                
                Spacer()
                
                Button(LocalizedString("close")) {
                    showSettings = false
                }
                .buttonStyle(ActionButtonStyle())
            }
            .padding()
            .tabItem {
                Label(LocalizedString("general_settings"), systemImage: "gearshape")
            }
            
            // Вторая вкладка с изменениями
            VStack(spacing: 0) {
                Text(LocalizedString("scripts_configuration"))
                    .font(.title)
                    .padding(.top)
                
                if scripts.isEmpty {
                    Text(LocalizedString("no_scripts"))
                        .foregroundColor(.secondary)
                        .padding()
                } else {
                    Text(LocalizedString("scripts_selected", enabledScriptsCount, scripts.count))
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                        .padding(.bottom, 12)
                    
                    // Отображаем выбранный скрипт, если есть
                    if let selectedScript = selectedScript {
                        Text("Selected: \(selectedScript.filename)")
                            .font(.headline)
                            .padding(.bottom, 8)
                    }
                    
                    HStack {
                        Button(LocalizedString("select_all")) {
                            scripts.indices.forEach { scripts[$0].isEnabled = true }
                        }
                        .buttonStyle(SecondaryButtonStyle())
                        
                        Button(LocalizedString("deselect_all")) {
                            scripts.indices.forEach { scripts[$0].isEnabled = false }
                        }
                        .buttonStyle(SecondaryButtonStyle())
                    }
                    .padding(.bottom)
                    
                    ScriptPickerView(scripts: $scripts, selectedScript: $selectedScript)
                        .frame(minHeight: 300)
                }
            }
            .padding(.horizontal)
            .tabItem {
                Label(LocalizedString("scripts_configuration"), systemImage: "scroll")
            }
        }
        .frame(width: 520, height: 450)
        .onAppear {
            selectedLanguage = LocalizationManager.shared.currentLanguage
            // Можно установить первый скрипт как выбранный по умолчанию
            selectedScript = scripts.first
        }
    }
}