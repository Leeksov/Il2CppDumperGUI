import Foundation

struct Script: Identifiable, Equatable, Hashable {
    let id = UUID()
    let filename: String
    var isEnabled: Bool
    var description: String
    
    static let availableScripts: [Script] = [
        Script(filename: "ghidra.py", isEnabled: false, description: "Ghidra basic analysis script"),
        Script(filename: "il2cpp_header_to_ghidra.py", isEnabled: false, description: "Ghidra with IL2CPP headers"),
        Script(filename: "il2cpp_header_to_binja.py", isEnabled: false, description: "Binary Ninja with IL2CPP headers"),
        Script(filename: "ida_with_struct_py3.py", isEnabled: false, description: "IDA Python 3 with structures"),
        Script(filename: "ida_with_struct.py", isEnabled: false, description: "IDA Python 2 with structures"),
        Script(filename: "ida_py3.py", isEnabled: false, description: "IDA Python 3 basic script"),
        Script(filename: "ida.py", isEnabled: false, description: "IDA Python 2 basic script"),
        Script(filename: "hopper-py3.py", isEnabled: false, description: "Hopper Python 3 script"),
        Script(filename: "ghidra_with_struct.py", isEnabled: false, description: "Ghidra with structure support"),
        Script(filename: "ghidra_wasm.py", isEnabled: false, description: "Ghidra WebAssembly support")
    ]
    
    var resourcePath: String? {
        Bundle.main.path(forResource: filename, ofType: nil)
    }
}
