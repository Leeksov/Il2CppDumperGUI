import Foundation
import SwiftUI

public enum Language: String, CaseIterable {
    case english = "en"
    case russian = "ru"
    public var displayName: String {
        switch self {
        case .english: return "English"
        case .russian: return "Русский"
        }
    }
}
public class LocalizationManager: ObservableObject {
    public static let shared = LocalizationManager()
    @Published public var currentLanguage: Language = .english {
        didSet {
            UserDefaults.standard.set(currentLanguage.rawValue, forKey: "selectedLanguage")
            Bundle.setLanguage(language: currentLanguage.rawValue)
            self.objectWillChange.send()
        }
    }
    private init() {
        if let langCode = UserDefaults.standard.string(forKey: "selectedLanguage"),
           let language = Language(rawValue: langCode) {
            currentLanguage = language
            Bundle.setLanguage(language: langCode)
        }
    }
    public func setLanguage(_ language: Language) {
        currentLanguage = language
    }
    public func localizedString(_ key: String) -> String {
        return Bundle.localizedString(forKey: key)
    }
    public func localizedString(_ key: String, _ args: CVarArg...) -> String {
        return String(format: localizedString(key), arguments: args)
    }
}
extension Bundle {
    private struct Holder {
        static var current: Bundle = .main
    }
    static var currentBundle: Bundle {
        get { Holder.current }
        set { Holder.current = newValue }
    }
    static func setLanguage(language code: String) {
        guard let path = Bundle.main.path(forResource: code, ofType: "lproj"),
              let bundle = Bundle(path: path) else { return }
        currentBundle = bundle
        object_setClass(Bundle.main, PrivateBundle.self)
    }
    public class func localizedString(forKey key: String) -> String {
        return currentBundle.localizedString(forKey: key, value: nil, table: nil)
    }
}
private class PrivateBundle: Bundle {
    override func localizedString(forKey key: String, value: String?, table: String?) -> String {
        return Bundle.currentBundle.localizedString(forKey: key, value: value, table: table)
    }
}
public func LocalizedString(_ key: String) -> String {
    return LocalizationManager.shared.localizedString(key)
}
public func LocalizedString(_ key: String, _ args: CVarArg...) -> String {
    return LocalizationManager.shared.localizedString(key, args)
}
