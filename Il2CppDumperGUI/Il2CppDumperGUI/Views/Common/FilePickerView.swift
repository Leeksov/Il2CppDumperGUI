import SwiftUI
import AppKit

struct FilePickerView: View {
    let titleKey: String
    @Binding var path: String
    var allowedFileExtensions: [String] = []
    var onFileSelected: ((String) -> Void)?
    
    var body: some View {
        HStack {
            TextField(LocalizedString(titleKey), text: $path)
                .textFieldStyle(RoundedBorderTextFieldStyle())
                .disabled(true)
            
            Button(LocalizedString("select_file"), action: selectFile)
                .buttonStyle(SecondaryButtonStyle())
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
