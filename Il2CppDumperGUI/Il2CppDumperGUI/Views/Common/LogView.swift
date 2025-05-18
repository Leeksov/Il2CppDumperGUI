import SwiftUI

struct LogView: View {
    @Binding var logText: String
    @Binding var scrollToBottomID: UUID
    @State private var showClearAlert = false
    @EnvironmentObject var localization: LocalizationManager
    
    var body: some View {
        VStack(spacing: 8) {
            clearLogButton
            logScrollView
        }
    }
    
    private var clearLogButton: some View {
        HStack {
            Spacer()
            Button(action: { showClearAlert = true }) {
                Image(systemName: "trash")
                    .foregroundColor(.red)
                    .font(.system(size: 14, weight: .bold))
            }
            .buttonStyle(PlainButtonStyle())
            .help(LocalizedString("clear_log"))
            .alert(isPresented: $showClearAlert) {
                Alert(
                    title: Text(LocalizedString("clear_log")),
                    message: Text(LocalizedString("confirm_clear_log")),
                    primaryButton: .destructive(Text(LocalizedString("clear"))) {
                        logText = Constants.defaultLogText
                    },
                    secondaryButton: .cancel(Text(LocalizedString("cancel")))
                )
            }
        }
        .padding(.horizontal, 4)
    }
    
    private var logScrollView: some View {
        ScrollViewReader { proxy in
            ScrollView {
                Text(logText)
                    .font(.system(.body, design: .monospaced))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(8)
                    .id(scrollToBottomID)
            }
            .background(Color.gray.opacity(0.1))
            .cornerRadius(6)
            .frame(maxHeight: .infinity)
            .onChange(of: logText) { _ in
                withAnimation(.easeOut(duration: 0.1)) {
                    proxy.scrollTo(scrollToBottomID, anchor: .bottom)
                }
            }
        }
    }
}
