import SwiftUI

@main
struct Il2CppDumperGUIApp: App {
    @StateObject private var localization = LocalizationManager.shared
    
    var body: some Scene {
        WindowGroup {
            MainView()
                .environmentObject(localization)
                .id(localization.currentLanguage)
                .frame(minWidth: 720, minHeight: 520)
        }
        .windowStyle(.titleBar)
        .commands {
            SidebarCommands()
            ToolbarCommands()
        }
    }
}
