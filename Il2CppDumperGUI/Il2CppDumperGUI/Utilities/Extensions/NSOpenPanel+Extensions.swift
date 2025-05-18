import AppKit

extension NSOpenPanel {
    func configureForFileSelection(title: String, allowedFileExtensions: [String]) {
        self.title = title
        self.allowedFileTypes = allowedFileExtensions
        self.allowsMultipleSelection = false
        self.canChooseFiles = true
        self.canChooseDirectories = false
    }
    func configureForDirectorySelection(title: String, directoryURL: URL?) {
        self.title = title
        self.directoryURL = directoryURL
        self.allowsMultipleSelection = false
        self.canChooseFiles = false
        self.canChooseDirectories = true
    }
}
