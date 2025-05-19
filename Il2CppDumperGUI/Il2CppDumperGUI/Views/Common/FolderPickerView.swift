import SwiftUI
import AppKit
import UniformTypeIdentifiers

struct FolderPickerView: View {
    let titleKey: String
    @Binding var path: String
    @State private var isTargeted = false
    var body: some View {
        HStack {
            TextField(LocalizedString(titleKey), text: $path)
                .textFieldStyle(RoundedBorderTextFieldStyle())
                .disabled(true)

            Button(LocalizedString("select_folder"), action: selectFolder)
                .buttonStyle(SecondaryButtonStyle())
        }
        .onDrop(of: [UTType.fileURL], isTargeted: $isTargeted) { providers in
            guard let provider = providers.first else { return false }
            _ = provider.loadDataRepresentation(forTypeIdentifier: UTType.fileURL.identifier) { data, _ in
                if let data,
                   let url = URL(dataRepresentation: data, relativeTo: nil,
                                 isAbsolute: true), url.hasDirectoryPath {
                    DispatchQueue.main.async {
                        path = url.path
                    }
                }
            }
            return true
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
