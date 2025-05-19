import SwiftUI
import AppKit
import UniformTypeIdentifiers

struct FilePickerView: View {
    let titleKey: String
    @Binding var path: String
    var allowedFileExtensions: [String] = []
    var onFileSelected: ((String) -> Void)?
    @State private var isTargeted = false
    
    var body: some View {
        HStack {
            TextField(LocalizedString(titleKey), text: $path)
                .textFieldStyle(RoundedBorderTextFieldStyle())
                .disabled(true)

            Button(LocalizedString("select_file"), action: selectFile)
                .buttonStyle(SecondaryButtonStyle())
        }
        .onDrop(of: [UTType.fileURL], isTargeted: $isTargeted) { providers in
            guard let provider = providers.first else { return false }
            _ = provider.loadDataRepresentation(forTypeIdentifier: UTType.fileURL.identifier) { data, _ in
                if let data,
                   let url = URL(dataRepresentation: data, relativeTo: nil,
                                 isAbsolute: true) {
                    DispatchQueue.main.async {
                        path = url.path
                        onFileSelected?(path)
                    }
                }
            }
            return true
        }
    }
    
    private func selectFile() {
        let dialog = NSOpenPanel()
        dialog.configureForFileSelection(
            title: LocalizedString(titleKey),
            allowedFileExtensions: allowedFileExtensions
        )
        
        if dialog.runModal() == .OK, let url = dialog.url {
            path = url.path
            onFileSelected?(path)
        }
    }
}
