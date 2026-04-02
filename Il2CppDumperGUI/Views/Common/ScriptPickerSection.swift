import SwiftUI

struct ScriptPopupPicker: View {
    @Binding var scripts: [Script]
    @State private var isExpanded = false
    @EnvironmentObject var localization: LocalizationManager
    
    private var sortedScripts: [Script] {
        scripts.sorted { $0.filename.localizedCaseInsensitiveCompare($1.filename) == .orderedAscending }
    }
    
    var selectedCountText: String {
        let selected = scripts.filter { $0.isEnabled }.count
        return LocalizedString("selected_count", selected, scripts.count)
    }
    
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            DisclosureGroup(isExpanded: $isExpanded) {
                VStack(alignment: .leading) {
                    ForEach($scripts) { $script in
                        Toggle(script.filename, isOn: $script.isEnabled)
                            .toggleStyle(.checkbox)
                            .font(.system(.body, design: .monospaced))
                            .padding(.vertical, 4)
                    }
                }
                .padding(.top, 8)
            } label: {
                HStack {
                    Text(selectedCountText)
                    Spacer()
                    Image(systemName: "chevron.down")
                        .rotationEffect(.degrees(isExpanded ? 180 : 0))
                }
                .contentShape(Rectangle())
            }
            .padding()
            .background(Color(.windowBackgroundColor))
            .cornerRadius(8)
            .overlay(
                RoundedRectangle(cornerRadius: 8)
                    .stroke(Color(.separatorColor), lineWidth: 1)
            )
        }
    }
}
