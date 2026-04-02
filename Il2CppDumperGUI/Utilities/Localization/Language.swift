//
//  Language.swift
//  Il2CppDumperGUI
//
//  Created by Leeksov on 10.5.2025.
//

import Foundation

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
