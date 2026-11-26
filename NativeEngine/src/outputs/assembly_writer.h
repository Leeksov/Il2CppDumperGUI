#pragma once


#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace il2cppdumper {

namespace ElementType {
constexpr uint8_t VOID = 0x01;
constexpr uint8_t BOOLEAN = 0x02;
constexpr uint8_t CHAR = 0x03;
constexpr uint8_t I1 = 0x04;
constexpr uint8_t U1 = 0x05;
constexpr uint8_t I2 = 0x06;
constexpr uint8_t U2 = 0x07;
constexpr uint8_t I4 = 0x08;
constexpr uint8_t U4 = 0x09;
constexpr uint8_t I8 = 0x0A;
constexpr uint8_t U8 = 0x0B;
constexpr uint8_t R4 = 0x0C;
constexpr uint8_t R8 = 0x0D;
constexpr uint8_t STRING = 0x0E;
constexpr uint8_t PTR = 0x0F;
constexpr uint8_t BYREF = 0x10;
constexpr uint8_t VALUETYPE = 0x11;
constexpr uint8_t CLASS = 0x12;
constexpr uint8_t VAR = 0x13;
constexpr uint8_t ARRAY = 0x14;
constexpr uint8_t GENERICINST = 0x15;
constexpr uint8_t TYPEDBYREF = 0x16;
constexpr uint8_t I = 0x18;
constexpr uint8_t U = 0x19;
constexpr uint8_t FNPTR = 0x1B;
constexpr uint8_t OBJECT = 0x1C;
constexpr uint8_t SZARRAY = 0x1D;
constexpr uint8_t MVAR = 0x1E;
constexpr uint8_t SENTINEL = 0x41;
}

namespace TokenType {
constexpr uint32_t TypeRef = 0x01000000;
constexpr uint32_t TypeDef = 0x02000000;
constexpr uint32_t Field = 0x04000000;
constexpr uint32_t MethodDef = 0x06000000;
constexpr uint32_t Param = 0x08000000;
constexpr uint32_t MemberRef = 0x0A000000;
constexpr uint32_t StandAloneSig = 0x11000000;
constexpr uint32_t TypeSpec = 0x1B000000;
}

namespace Op {
constexpr uint8_t NOP = 0x00;
constexpr uint8_t LDNULL = 0x14;
constexpr uint8_t LDLOC_0 = 0x06;
constexpr uint8_t LDLOCA_S = 0x11;
constexpr uint8_t RET = 0x2A;
constexpr uint8_t INITOBJ = 0xFE;
constexpr uint8_t INITOBJ_TAG = 0x15;
}

namespace TableId {
constexpr uint32_t Module = 0x00;
constexpr uint32_t TypeRef = 0x01;
constexpr uint32_t TypeDef = 0x02;
constexpr uint32_t Field = 0x04;
constexpr uint32_t MethodDef = 0x06;
constexpr uint32_t Param = 0x08;
constexpr uint32_t InterfaceImpl = 0x09;
constexpr uint32_t MemberRef = 0x0A;
constexpr uint32_t Constant = 0x0B;
constexpr uint32_t CustomAttribute = 0x0C;
constexpr uint32_t StandAloneSig = 0x11;
constexpr uint32_t EventMap = 0x12;
constexpr uint32_t Event = 0x14;
constexpr uint32_t PropertyMap = 0x15;
constexpr uint32_t Property = 0x17;
constexpr uint32_t MethodSemantics = 0x18;
constexpr uint32_t TypeSpec = 0x1B;
constexpr uint32_t Assembly = 0x20;
constexpr uint32_t AssemblyRef = 0x23;
constexpr uint32_t NestedClass = 0x29;
constexpr uint32_t GenericParam = 0x2A;
constexpr uint32_t MethodSpec = 0x2B;
constexpr uint32_t GenericParamConstraint = 0x2C;
}

namespace MethodSemanticsAttr {
constexpr uint16_t Setter = 0x0001;
constexpr uint16_t Getter = 0x0002;
constexpr uint16_t Other = 0x0004;
constexpr uint16_t AddOn = 0x0008;
constexpr uint16_t RemoveOn = 0x0010;
constexpr uint16_t Fire = 0x0020;
}

namespace CorElementType {
constexpr uint8_t BOOLEAN = 0x02;
constexpr uint8_t CHAR = 0x03;
constexpr uint8_t I1 = 0x04;
constexpr uint8_t U1 = 0x05;
constexpr uint8_t I2 = 0x06;
constexpr uint8_t U2 = 0x07;
constexpr uint8_t I4 = 0x08;
constexpr uint8_t U4 = 0x09;
constexpr uint8_t I8 = 0x0A;
constexpr uint8_t U8 = 0x0B;
constexpr uint8_t R4 = 0x0C;
constexpr uint8_t R8 = 0x0D;
constexpr uint8_t STRING = 0x0E;
constexpr uint8_t CLASS = 0x12;
}

class AssemblyWriter {
public:
    AssemblyWriter() = default;
    AssemblyWriter(const std::string& assemblyName, const std::string& moduleName, uint16_t major,
                   uint16_t minor, uint16_t build, uint16_t revision, bool is32Bit = false);

    uint32_t addTypeDef(const std::string& ns, const std::string& name, uint32_t flags, uint32_t extends,
                        int fieldCount, int methodCount, int eventCount, int propertyCount);
    uint32_t addTypeRef(uint32_t resolutionScope, const std::string& ns, const std::string& name);
    uint32_t addField(uint16_t flags, const std::string& name, uint32_t sigBlob);
    uint32_t addMethodDef(uint16_t implFlags, uint16_t flags, const std::string& name, uint32_t sigBlob,
                          int paramCount);
    uint32_t addParam(uint16_t flags, uint16_t sequence, const std::string& name);
    uint32_t addMemberRef(uint32_t parent, const std::string& name, uint32_t sigBlob);
    uint32_t addInterfaceImpl(uint32_t classRow, uint32_t interfaceCoded);
    uint32_t addConstant(uint8_t type, uint32_t parentCoded, uint32_t valueBlob);
    uint32_t addCustomAttribute(uint32_t parentCoded, uint32_t ctorCoded, uint32_t valueBlob);
    uint32_t addEventMap(uint32_t typeDefRow);
    uint32_t addEvent(uint16_t flags, const std::string& name, uint32_t eventTypeCoded);
    uint32_t addPropertyMap(uint32_t typeDefRow);
    uint32_t addProperty(uint16_t flags, const std::string& name, uint32_t typeBlob);
    void addMethodSemantics(uint16_t semantics, uint32_t methodRow, uint32_t associationCoded);
    uint32_t addTypeSpec(uint32_t sigBlob);
    uint32_t addStandAloneSig(uint32_t sigBlob);
    uint32_t addAssemblyRef(uint16_t major, uint16_t minor, uint16_t build, uint16_t revision, uint32_t flags,
                            uint32_t publicKeyOrTokenBlob, const std::string& name, const std::string& culture,
                            uint32_t hashBlob);
    void addNestedClass(uint32_t nestedRow, uint32_t enclosingRow);
    uint32_t addGenericParam(uint16_t number, uint16_t flags, uint32_t ownerCoded, const std::string& name);
    void addGenericParamConstraint(uint32_t ownerRow, uint32_t constraintCoded);
    uint32_t addMethodSpec(uint32_t methodCoded, uint32_t instantiationBlob);

    void setAssembly(uint32_t hashAlgId, uint16_t major, uint16_t minor, uint16_t build, uint16_t revision,
                     uint32_t flags, uint32_t publicKeyBlob, const std::string& name, const std::string& culture);

    uint32_t stringIndex(const std::string& s);
    uint32_t blobIndex(const std::vector<uint8_t>& b);
    uint32_t guidIndex();
    uint32_t userStringIndex(const std::string& s);

    uint32_t typeSig(uint8_t type, uint32_t typeDefOrRefCoded);
    uint32_t typeSigPtr(uint8_t type, uint32_t typeDefOrRefCoded);
    uint32_t typeSigByRef(uint8_t type, uint32_t typeDefOrRefCoded);
    uint32_t typeSigSzArray(uint8_t elem, uint32_t typeDefOrRefCoded);
    uint32_t typeSigGenericVar(uint8_t varOrMvar, uint16_t number);
    uint32_t methodSig(bool hasThis, bool generic, uint16_t genericParamCount,
                       const std::vector<uint8_t>& retType, const std::vector<std::vector<uint8_t>>& paramTypes);
    uint32_t fieldSig(const std::vector<uint8_t>& fieldType);

    uint32_t typeDefToken(uint32_t row) const { return TokenType::TypeDef | row; }
    uint32_t typeRefToken(uint32_t row) const { return TokenType::TypeRef | row; }
    uint32_t fieldToken(uint32_t row) const { return TokenType::Field | row; }
    uint32_t methodDefToken(uint32_t row) const { return TokenType::MethodDef | row; }
    uint32_t memberRefToken(uint32_t row) const { return TokenType::MemberRef | row; }
    uint32_t typeSpecToken(uint32_t row) const { return TokenType::TypeSpec | row; }
    uint32_t paramToken(uint32_t row) const { return TokenType::Param | row; }
    uint32_t userStringToken(uint32_t usIndex) const { return 0x70000000 | (usIndex << 1); }

    void setMethodBody(uint32_t methodDefRow, const std::vector<uint8_t>& il);
    void setMethodBody(uint32_t methodDefRow, const std::vector<uint8_t>& il, uint32_t localsSigBlob);

    std::vector<uint8_t> write();

private:
    struct TypeDefRow {
        uint32_t flags = 0;
        uint32_t name = 0;
        uint32_t ns = 0;
        uint32_t extends = 0;
        int fieldCount = 0;
        int methodCount = 0;
        int eventCount = 0;
        int propertyCount = 0;
    };
    struct TypeRefRow {
        uint32_t scope = 0;
        uint32_t name = 0;
        uint32_t ns = 0;
    };
    struct FieldRow {
        uint16_t flags = 0;
        uint32_t name = 0;
        uint32_t signature = 0;
    };
    struct MethodDefRow {
        uint32_t rva = 0;
        uint16_t implFlags = 0;
        uint16_t flags = 0;
        uint32_t name = 0;
        uint32_t signature = 0;
        uint32_t paramList = 0;
        int paramCount = 0;
    };
    struct ParamRow {
        uint16_t flags = 0;
        uint16_t sequence = 0;
        uint32_t name = 0;
    };
    struct InterfaceImplRow {
        uint32_t classRow = 0;
        uint32_t interfaceCoded = 0;
    };
    struct MemberRefRow {
        uint32_t parent = 0;
        uint32_t name = 0;
        uint32_t signature = 0;
    };
    struct ConstantRow {
        uint8_t type = 0;
        uint32_t parent = 0;
        uint32_t value = 0;
    };
    struct CustomAttributeRow {
        uint32_t parent = 0;
        uint32_t ctor = 0;
        uint32_t value = 0;
    };
    struct EventMapRow {
        uint32_t parent = 0;
        uint32_t eventList = 0;
    };
    struct EventRow {
        uint16_t flags = 0;
        uint32_t name = 0;
        uint32_t eventType = 0;
    };
    struct PropertyMapRow {
        uint32_t parent = 0;
        uint32_t propertyList = 0;
    };
    struct PropertyRow {
        uint16_t flags = 0;
        uint32_t name = 0;
        uint32_t type = 0;
    };
    struct MethodSemanticsRow {
        uint16_t semantics = 0;
        uint32_t method = 0;
        uint32_t association = 0;
    };
    struct TypeSpecRow {
        uint32_t signature = 0;
    };
    struct StandAloneSigRow {
        uint32_t signature = 0;
    };
    struct AssemblyRefRow {
        uint16_t major = 0, minor = 0, build = 0, revision = 0;
        uint32_t flags = 0;
        uint32_t publicKeyOrToken = 0;
        uint32_t name = 0;
        uint32_t culture = 0;
        uint32_t hashValue = 0;
    };
    struct NestedClassRow {
        uint32_t nested = 0;
        uint32_t enclosing = 0;
    };
    struct GenericParamRow {
        uint16_t number = 0;
        uint16_t flags = 0;
        uint32_t owner = 0;
        uint32_t name = 0;
    };
    struct MethodSpecRow {
        uint32_t method = 0;
        uint32_t instantiation = 0;
    };
    struct GenericParamConstraintRow {
        uint32_t owner = 0;
        uint32_t constraint = 0;
    };
    struct MethodBody {
        std::vector<uint8_t> il;
        uint32_t localsSigBlob = 0;
    };

    int simpleIndexBytes(uint32_t rowCount) const { return rowCount >= 0x10000 ? 4 : 2; }
    int codedIndexBytes(uint32_t maxRowCount, int tagBits) const {
        return maxRowCount < (1u << (16 - tagBits)) ? 2 : 4;
    }
    std::vector<uint8_t> serialize() const;
    void serializeTables(std::vector<uint8_t>& out) const;

    std::string assemblyName_;
    std::string moduleName_;
    uint32_t moduleNameIdx_ = 0;

    std::map<std::string, uint32_t> stringHeap_;
    std::vector<std::string> stringList_;
    uint32_t stringSize_ = 1;
    std::map<std::vector<uint8_t>, uint32_t> blobHeap_;
    std::vector<std::vector<uint8_t>> blobList_;
    uint32_t blobSize_ = 1;
    std::vector<std::string> userStrings_;
    std::map<std::string, uint32_t> userStringHeap_;
    uint32_t usSize_ = 1;
    uint32_t guidCount_ = 0;

    std::vector<TypeDefRow> typeDefs_;
    std::vector<TypeRefRow> typeRefs_;
    std::vector<FieldRow> fields_;
    std::vector<MethodDefRow> methodDefs_;
    std::vector<ParamRow> params_;
    std::vector<InterfaceImplRow> interfaceImpls_;
    std::vector<MemberRefRow> memberRefs_;
    std::vector<ConstantRow> constants_;
    std::vector<CustomAttributeRow> customAttributes_;
    std::vector<EventMapRow> eventMaps_;
    std::vector<EventRow> events_;
    std::vector<PropertyMapRow> propertyMaps_;
    std::vector<PropertyRow> properties_;
    std::vector<MethodSemanticsRow> methodSemantics_;
    std::vector<TypeSpecRow> typeSpecs_;
    std::vector<StandAloneSigRow> standAloneSigs_;
    std::vector<AssemblyRefRow> assemblyRefs_;
    std::vector<NestedClassRow> nestedClasses_;
    std::vector<GenericParamRow> genericParams_;
    std::vector<MethodSpecRow> methodSpecs_;
    std::vector<GenericParamConstraintRow> genericParamConstraints_;

    bool hasAssembly_ = false;
    uint32_t assemblyHashAlg_ = 0;
    uint16_t assemblyMajor_ = 0, assemblyMinor_ = 0, assemblyBuild_ = 0, assemblyRevision_ = 0;
    uint32_t assemblyFlags_ = 0;
    uint32_t assemblyPublicKey_ = 0;
    uint32_t asmName_ = 0;
    uint32_t asmCulture_ = 0;

    std::map<uint32_t, MethodBody> bodies_;
};

namespace Sig {
void pushCompressedU(std::vector<uint8_t>& out, uint32_t v);
void pushCompressedI(std::vector<uint8_t>& out, int32_t v);
void pushSerString(std::vector<uint8_t>& out, const std::string& s);
}

}
