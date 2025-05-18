import SwiftUI

@main
struct Il2CppDumperGUIApp: App {
    @StateObject private var localization = LocalizationManager.shared
    
    var body: some Scene {
        WindowGroup {
            MainView()
                .environmentObject(localization)
                .id(localization.currentLanguage)
                .frame(minWidth: 800, minHeight: 600)
                .handlesExternalEvents(preferring: ["*"], allowing: ["*"])
        }
        .windowStyle(.hiddenTitleBar)
        .commands {
            SidebarCommands()
            ToolbarCommands()
        }
    }
}
