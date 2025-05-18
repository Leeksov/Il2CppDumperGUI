import SwiftUI
import AppKit

struct FolderPickerView: View {
    let titleKey: String
    @Binding var path: String
    var body: some View {
        HStack {
            TextField(LocalizedString(titleKey), text: $path)
                .textFieldStyle(RoundedBorderTextFieldStyle())
                .disabled(true)
            
            Button(LocalizedString("select_folder"), action: selectFolder)
                .buttonStyle(SecondaryButtonStyle())
        }
    }
    private func selectFolder() {
        let dialog = NSOpenPanel()
        dialog.configureForDirectorySelection(
            title: LocalizedString(titleKey),
            directoryURL: nil
        )
        if dialog.runModal() == .OK, let url = dialog.url {
            path = url.path
        }
    }
}
