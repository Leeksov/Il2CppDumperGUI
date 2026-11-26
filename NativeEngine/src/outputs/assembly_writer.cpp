#include "outputs/assembly_writer.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <stdexcept>

namespace il2cppdumper {

namespace {

inline uint32_t alignUp(uint32_t v, uint32_t a) {
    return (v + a - 1) & ~(a - 1);
}

struct ByteBuf {
    std::vector<uint8_t> b;

    void u8(uint8_t v) { b.push_back(v); }
    void u16(uint16_t v) {
        b.push_back(static_cast<uint8_t>(v & 0xFF));
        b.push_back(static_cast<uint8_t>(v >> 8));
    }
    void u32(uint32_t v) {
        b.push_back(static_cast<uint8_t>(v & 0xFF));
        b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }
    void u64(uint64_t v) {
        for (int i = 0; i < 8; i++) {
            b.push_back(static_cast<uint8_t>(v >> (8 * i)));
        }
    }
    void raw(const uint8_t* p, size_t n) { b.insert(b.end(), p, p + n); }
    void raw(const char* s, size_t n) { raw(reinterpret_cast<const uint8_t*>(s), n); }
    void pad(size_t n) { b.insert(b.end(), n, 0); }
    void u32At(size_t off, uint32_t v) {
        b[off] = static_cast<uint8_t>(v & 0xFF);
        b[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        b[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        b[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    }
};

std::vector<uint8_t> makeMvid(const std::string& moduleName, const std::string& assemblyName) {
    uint64_t h1 = 1469598103934665603ull;
    uint64_t h2 = 1099511628211ull;
    auto mix = [&h1, &h2](char c) {
        h1 = (h1 ^ static_cast<uint8_t>(c)) * 1099511628211ull;
        h2 = (h2 * 31ull + static_cast<uint8_t>(c)) ^ 0x9e3779b97f4a7c15ull;
    };
    for (char c : moduleName) {
        mix(c);
    }
    for (char c : assemblyName) {
        mix(c);
    }
    std::vector<uint8_t> g(16);
    for (int i = 0; i < 8; i++) {
        g[static_cast<size_t>(i)] = static_cast<uint8_t>(h1 >> (8 * i));
        g[static_cast<size_t>(i + 8)] = static_cast<uint8_t>(h2 >> (8 * i));
    }
    return g;
}

}


AssemblyWriter::AssemblyWriter(const std::string& assemblyName, const std::string& moduleName,
                               uint16_t /*major*/, uint16_t /*minor*/, uint16_t /*build*/,
                               uint16_t /*revision*/, bool /*is32Bit*/)
    : assemblyName_(assemblyName), moduleName_(moduleName) {
    moduleNameIdx_ = stringIndex(moduleName_);
}

uint32_t AssemblyWriter::stringIndex(const std::string& s) {
    if (s.empty()) {
        return 0;
    }
    auto it = stringHeap_.find(s);
    if (it != stringHeap_.end()) {
        return it->second;
    }
    uint32_t idx = stringSize_;
    stringHeap_[s] = idx;
    stringList_.push_back(s);
    stringSize_ += static_cast<uint32_t>(s.size()) + 1;
    return idx;
}

uint32_t AssemblyWriter::blobIndex(const std::vector<uint8_t>& b) {
    if (b.empty()) {
        return 0;
    }
    auto it = blobHeap_.find(b);
    if (it != blobHeap_.end()) {
        return it->second;
    }
    uint32_t idx = blobSize_;
    blobHeap_[b] = idx;
    blobList_.push_back(b);
    blobSize_ += 1 + static_cast<uint32_t>(b.size());
    std::vector<uint8_t> tmp;
    Sig::pushCompressedU(tmp, static_cast<uint32_t>(b.size()));
    blobSize_ += static_cast<uint32_t>(tmp.size()) - 1;
    return idx;
}

uint32_t AssemblyWriter::guidIndex() { return 1; }

uint32_t AssemblyWriter::userStringIndex(const std::string& s) {
    auto it = userStringHeap_.find(s);
    if (it != userStringHeap_.end()) {
        return it->second;
    }
    uint32_t idx = usSize_;
    userStringHeap_[s] = idx;
    userStrings_.push_back(s);
    usSize_ += 1 + static_cast<uint32_t>(s.size() * 2 + 1);
    return idx;
}


uint32_t AssemblyWriter::addTypeDef(const std::string& ns, const std::string& name, uint32_t flags,
                                    uint32_t extends, int fieldCount, int methodCount, int eventCount,
                                    int propertyCount) {
    TypeDefRow r;
    r.flags = flags;
    r.name = stringIndex(name);
    r.ns = stringIndex(ns);
    r.extends = extends;
    r.fieldCount = fieldCount;
    r.methodCount = methodCount;
    r.eventCount = eventCount;
    r.propertyCount = propertyCount;
    typeDefs_.push_back(r);
    return static_cast<uint32_t>(typeDefs_.size());
}

uint32_t AssemblyWriter::addTypeRef(uint32_t resolutionScope, const std::string& ns, const std::string& name) {
    TypeRefRow r;
    r.scope = resolutionScope;
    r.name = stringIndex(name);
    r.ns = stringIndex(ns);
    typeRefs_.push_back(r);
    return static_cast<uint32_t>(typeRefs_.size());
}

uint32_t AssemblyWriter::addField(uint16_t flags, const std::string& name, uint32_t sigBlob) {
    FieldRow r;
    r.flags = flags;
    r.name = stringIndex(name);
    r.signature = sigBlob;
    fields_.push_back(r);
    return static_cast<uint32_t>(fields_.size());
}

uint32_t AssemblyWriter::addMethodDef(uint16_t implFlags, uint16_t flags, const std::string& name,
                                      uint32_t sigBlob, int paramCount) {
    MethodDefRow r;
    r.implFlags = implFlags;
    r.flags = flags;
    r.name = stringIndex(name);
    r.signature = sigBlob;
    r.paramCount = paramCount;
    methodDefs_.push_back(r);
    return static_cast<uint32_t>(methodDefs_.size());
}

uint32_t AssemblyWriter::addParam(uint16_t flags, uint16_t sequence, const std::string& name) {
    ParamRow r;
    r.flags = flags;
    r.sequence = sequence;
    r.name = stringIndex(name);
    params_.push_back(r);
    return static_cast<uint32_t>(params_.size());
}

uint32_t AssemblyWriter::addMemberRef(uint32_t parent, const std::string& name, uint32_t sigBlob) {
    MemberRefRow r;
    r.parent = parent;
    r.name = stringIndex(name);
    r.signature = sigBlob;
    memberRefs_.push_back(r);
    return static_cast<uint32_t>(memberRefs_.size());
}

uint32_t AssemblyWriter::addInterfaceImpl(uint32_t classRow, uint32_t interfaceCoded) {
    InterfaceImplRow r;
    r.classRow = classRow;
    r.interfaceCoded = interfaceCoded;
    interfaceImpls_.push_back(r);
    return static_cast<uint32_t>(interfaceImpls_.size());
}

uint32_t AssemblyWriter::addConstant(uint8_t type, uint32_t parentCoded, uint32_t valueBlob) {
    ConstantRow r;
    r.type = type;
    r.parent = parentCoded;
    r.value = valueBlob;
    constants_.push_back(r);
    return static_cast<uint32_t>(constants_.size());
}

uint32_t AssemblyWriter::addCustomAttribute(uint32_t parentCoded, uint32_t ctorCoded, uint32_t valueBlob) {
    CustomAttributeRow r;
    r.parent = parentCoded;
    r.ctor = ctorCoded;
    r.value = valueBlob;
    customAttributes_.push_back(r);
    return static_cast<uint32_t>(customAttributes_.size());
}

uint32_t AssemblyWriter::addEventMap(uint32_t typeDefRow) {
    EventMapRow r;
    r.parent = typeDefRow;
    eventMaps_.push_back(r);
    return static_cast<uint32_t>(eventMaps_.size());
}

uint32_t AssemblyWriter::addEvent(uint16_t flags, const std::string& name, uint32_t eventTypeCoded) {
    EventRow r;
    r.flags = flags;
    r.name = stringIndex(name);
    r.eventType = eventTypeCoded;
    events_.push_back(r);
    return static_cast<uint32_t>(events_.size());
}

uint32_t AssemblyWriter::addPropertyMap(uint32_t typeDefRow) {
    PropertyMapRow r;
    r.parent = typeDefRow;
    propertyMaps_.push_back(r);
    return static_cast<uint32_t>(propertyMaps_.size());
}

uint32_t AssemblyWriter::addProperty(uint16_t flags, const std::string& name, uint32_t typeBlob) {
    PropertyRow r;
    r.flags = flags;
    r.name = stringIndex(name);
    r.type = typeBlob;
    properties_.push_back(r);
    return static_cast<uint32_t>(properties_.size());
}

void AssemblyWriter::addMethodSemantics(uint16_t semantics, uint32_t methodRow, uint32_t associationCoded) {
    MethodSemanticsRow r;
    r.semantics = semantics;
    r.method = methodRow;
    r.association = associationCoded;
    methodSemantics_.push_back(r);
}

uint32_t AssemblyWriter::addTypeSpec(uint32_t sigBlob) {
    TypeSpecRow r;
    r.signature = sigBlob;
    typeSpecs_.push_back(r);
    return static_cast<uint32_t>(typeSpecs_.size());
}

uint32_t AssemblyWriter::addStandAloneSig(uint32_t sigBlob) {
    StandAloneSigRow r;
    r.signature = sigBlob;
    standAloneSigs_.push_back(r);
    return static_cast<uint32_t>(standAloneSigs_.size());
}

uint32_t AssemblyWriter::addAssemblyRef(uint16_t major, uint16_t minor, uint16_t build, uint16_t revision,
                                        uint32_t flags, uint32_t publicKeyOrTokenBlob, const std::string& name,
                                        const std::string& culture, uint32_t hashBlob) {
    AssemblyRefRow r;
    r.major = major;
    r.minor = minor;
    r.build = build;
    r.revision = revision;
    r.flags = flags;
    r.publicKeyOrToken = publicKeyOrTokenBlob;
    r.name = stringIndex(name);
    r.culture = stringIndex(culture);
    r.hashValue = hashBlob;
    assemblyRefs_.push_back(r);
    return static_cast<uint32_t>(assemblyRefs_.size());
}

void AssemblyWriter::addNestedClass(uint32_t nestedRow, uint32_t enclosingRow) {
    NestedClassRow r;
    r.nested = nestedRow;
    r.enclosing = enclosingRow;
    nestedClasses_.push_back(r);
}

uint32_t AssemblyWriter::addGenericParam(uint16_t number, uint16_t flags, uint32_t ownerCoded,
                                         const std::string& name) {
    GenericParamRow r;
    r.number = number;
    r.flags = flags;
    r.owner = ownerCoded;
    r.name = stringIndex(name);
    genericParams_.push_back(r);
    return static_cast<uint32_t>(genericParams_.size());
}

void AssemblyWriter::addGenericParamConstraint(uint32_t ownerRow, uint32_t constraintCoded) {
    GenericParamConstraintRow r;
    r.owner = ownerRow;
    r.constraint = constraintCoded;
    genericParamConstraints_.push_back(r);
}

uint32_t AssemblyWriter::addMethodSpec(uint32_t methodCoded, uint32_t instantiationBlob) {
    MethodSpecRow r;
    r.method = methodCoded;
    r.instantiation = instantiationBlob;
    methodSpecs_.push_back(r);
    return static_cast<uint32_t>(methodSpecs_.size());
}

void AssemblyWriter::setAssembly(uint32_t hashAlgId, uint16_t major, uint16_t minor, uint16_t build,
                                 uint16_t revision, uint32_t flags, uint32_t publicKeyBlob,
                                 const std::string& name, const std::string& culture) {
    hasAssembly_ = true;
    assemblyHashAlg_ = hashAlgId;
    assemblyMajor_ = major;
    assemblyMinor_ = minor;
    assemblyBuild_ = build;
    assemblyRevision_ = revision;
    assemblyFlags_ = flags;
    assemblyPublicKey_ = publicKeyBlob;
    asmName_ = stringIndex(name);
    asmCulture_ = stringIndex(culture);
}


namespace {

void pushTypeBytes(std::vector<uint8_t>& out, uint8_t type, uint32_t typeDefOrRefCoded) {
    out.push_back(type);
    if (type == ElementType::VALUETYPE || type == ElementType::CLASS) {
        Sig::pushCompressedU(out, typeDefOrRefCoded);
    }
}

}

uint32_t AssemblyWriter::typeSig(uint8_t type, uint32_t typeDefOrRefCoded) {
    std::vector<uint8_t> b;
    pushTypeBytes(b, type, typeDefOrRefCoded);
    return blobIndex(b);
}

uint32_t AssemblyWriter::typeSigPtr(uint8_t type, uint32_t typeDefOrRefCoded) {
    std::vector<uint8_t> b = {ElementType::PTR};
    pushTypeBytes(b, type, typeDefOrRefCoded);
    return blobIndex(b);
}

uint32_t AssemblyWriter::typeSigByRef(uint8_t type, uint32_t typeDefOrRefCoded) {
    std::vector<uint8_t> b = {ElementType::BYREF};
    pushTypeBytes(b, type, typeDefOrRefCoded);
    return blobIndex(b);
}

uint32_t AssemblyWriter::typeSigSzArray(uint8_t elem, uint32_t typeDefOrRefCoded) {
    std::vector<uint8_t> b = {ElementType::SZARRAY};
    pushTypeBytes(b, elem, typeDefOrRefCoded);
    return blobIndex(b);
}

uint32_t AssemblyWriter::typeSigGenericVar(uint8_t varOrMvar, uint16_t number) {
    std::vector<uint8_t> b = {varOrMvar};
    Sig::pushCompressedU(b, number);
    return blobIndex(b);
}

uint32_t AssemblyWriter::methodSig(bool hasThis, bool generic, uint16_t genericParamCount,
                                   const std::vector<uint8_t>& retType,
                                   const std::vector<std::vector<uint8_t>>& paramTypes) {
    std::vector<uint8_t> b;
    uint8_t conv = generic ? 0x10 : 0x00;
    if (hasThis) {
        conv |= 0x20;
    }
    b.push_back(conv);
    if (generic) {
        Sig::pushCompressedU(b, genericParamCount);
    }
    Sig::pushCompressedU(b, static_cast<uint32_t>(paramTypes.size()));
    b.insert(b.end(), retType.begin(), retType.end());
    for (const auto& p : paramTypes) {
        b.insert(b.end(), p.begin(), p.end());
    }
    return blobIndex(b);
}

uint32_t AssemblyWriter::fieldSig(const std::vector<uint8_t>& fieldType) {
    std::vector<uint8_t> b = {0x06};
    b.insert(b.end(), fieldType.begin(), fieldType.end());
    return blobIndex(b);
}

void AssemblyWriter::setMethodBody(uint32_t methodDefRow, const std::vector<uint8_t>& il) {
    MethodBody body;
    body.il = il;
    bodies_[methodDefRow] = std::move(body);
}

void AssemblyWriter::setMethodBody(uint32_t methodDefRow, const std::vector<uint8_t>& il,
                                   uint32_t localsSigBlob) {
    MethodBody body;
    body.il = il;
    body.localsSigBlob = localsSigBlob;
    bodies_[methodDefRow] = std::move(body);
}


void AssemblyWriter::serializeTables(std::vector<uint8_t>& out) const {
    auto tableCount = [](const auto& v) { return static_cast<uint32_t>(v.size()); };
    uint32_t nModule = 1;
    uint32_t nTypeRef = tableCount(typeRefs_);
    uint32_t nTypeDef = tableCount(typeDefs_);
    uint32_t nField = tableCount(fields_);
    uint32_t nMethodDef = tableCount(methodDefs_);
    uint32_t nParam = tableCount(params_);
    uint32_t nInterfaceImpl = tableCount(interfaceImpls_);
    uint32_t nMemberRef = tableCount(memberRefs_);
    uint32_t nConstant = tableCount(constants_);
    uint32_t nCustomAttribute = tableCount(customAttributes_);
    uint32_t nEventMap = tableCount(eventMaps_);
    uint32_t nEvent = tableCount(events_);
    uint32_t nPropertyMap = tableCount(propertyMaps_);
    uint32_t nProperty = tableCount(properties_);
    uint32_t nMethodSemantics = tableCount(methodSemantics_);
    uint32_t nTypeSpec = tableCount(typeSpecs_);
    uint32_t nStandAloneSig = tableCount(standAloneSigs_);
    uint32_t nAssembly = hasAssembly_ ? 1 : 0;
    uint32_t nAssemblyRef = tableCount(assemblyRefs_);
    uint32_t nNestedClass = tableCount(nestedClasses_);
    uint32_t nGenericParam = tableCount(genericParams_);
    uint32_t nMethodSpec = tableCount(methodSpecs_);
    uint32_t nGenericParamConstraint = tableCount(genericParamConstraints_);

    int strBytes = stringSize_ >= 0x10000 ? 4 : 2;
    int guidBytes = guidCount_ >= 0x10000 ? 4 : 2;
    int blobBytes = blobSize_ >= 0x10000 ? 4 : 2;
    int tdorBytes = codedIndexBytes(std::max({nTypeDef, nTypeRef, nTypeSpec}), 2);
    int hasConstantBytes = codedIndexBytes(std::max({nField, nParam, nProperty}), 2);
    int hasCustomAttributeBytes =
        codedIndexBytes(std::max({nMethodDef, nField, nTypeRef, nTypeDef, nParam, nMemberRef, nProperty, nEvent}),
                        5);
    int memberRefParentBytes = codedIndexBytes(std::max({nTypeDef, nTypeRef, nMethodDef}), 3);
    int hasSemanticsBytes = codedIndexBytes(std::max(nEvent, nProperty), 1);
    int typeOrMethodDefBytes = codedIndexBytes(std::max(nTypeDef, nMethodDef), 1);
    int methodDefOrRefBytes = codedIndexBytes(std::max(nMethodDef, nMemberRef), 1);
    int resolutionScopeBytes = codedIndexBytes(std::max(nAssemblyRef, nTypeRef), 2);
    int customAttributeTypeBytes = codedIndexBytes(std::max(nMethodDef, nMemberRef), 3);

    uint64_t valid = 0;
    uint64_t sorted = 0;
    auto mark = [&valid](uint32_t tableId) { valid |= (1ull << tableId); };
    mark(TableId::Module);
    if (nTypeRef) mark(TableId::TypeRef);
    if (nTypeDef) mark(TableId::TypeDef);
    if (nField) mark(TableId::Field);
    if (nMethodDef) mark(TableId::MethodDef);
    if (nParam) mark(TableId::Param);
    if (nInterfaceImpl) mark(TableId::InterfaceImpl);
    if (nMemberRef) mark(TableId::MemberRef);
    if (nConstant) mark(TableId::Constant);
    if (nCustomAttribute) mark(TableId::CustomAttribute);
    if (nEventMap) mark(TableId::EventMap);
    if (nEvent) mark(TableId::Event);
    if (nPropertyMap) mark(TableId::PropertyMap);
    if (nProperty) mark(TableId::Property);
    if (nMethodSemantics) mark(TableId::MethodSemantics);
    if (nTypeSpec) mark(TableId::TypeSpec);
    if (nStandAloneSig) mark(TableId::StandAloneSig);
    if (nAssembly) mark(TableId::Assembly);
    if (nAssemblyRef) mark(TableId::AssemblyRef);
    if (nNestedClass) mark(TableId::NestedClass);
    if (nGenericParam) mark(TableId::GenericParam);
    if (nMethodSpec) mark(TableId::MethodSpec);
    if (nGenericParamConstraint) mark(TableId::GenericParamConstraint);
    sorted = (1ull << TableId::InterfaceImpl) | (1ull << TableId::Constant) | (1ull << TableId::CustomAttribute) |
             (1ull << TableId::MethodSemantics) | (1ull << TableId::NestedClass) | (1ull << TableId::GenericParam) |
             (1ull << TableId::GenericParamConstraint);

    ByteBuf t;
    t.u32(0);
    t.u8(2);
    t.u8(0);
    uint8_t heapSizes = 0;
    if (stringSize_ >= 0x10000) {
        heapSizes |= 0x01;
    }
    if (guidCount_ >= 0x10000) {
        heapSizes |= 0x02;
    }
    if (blobSize_ >= 0x10000) {
        heapSizes |= 0x04;
    }
    t.u8(heapSizes);
    t.u8(1);
    t.u64(valid);
    t.u64(sorted);

    auto writeIndex = [&t](uint32_t value, int bytes) {
        if (bytes == 4) {
            t.b.push_back(static_cast<uint8_t>(value & 0xFF));
            t.b.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            t.b.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            t.b.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        } else {
            t.b.push_back(static_cast<uint8_t>(value & 0xFF));
            t.b.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }
    };

    auto pushCount = [&t](uint32_t count) { t.u32(count); };
    pushCount(nModule);
    if (nTypeRef) pushCount(nTypeRef);
    if (nTypeDef) pushCount(nTypeDef);
    if (nField) pushCount(nField);
    if (nMethodDef) pushCount(nMethodDef);
    if (nParam) pushCount(nParam);
    if (nInterfaceImpl) pushCount(nInterfaceImpl);
    if (nMemberRef) pushCount(nMemberRef);
    if (nConstant) pushCount(nConstant);
    if (nCustomAttribute) pushCount(nCustomAttribute);
    if (nStandAloneSig) pushCount(nStandAloneSig);
    if (nEventMap) pushCount(nEventMap);
    if (nEvent) pushCount(nEvent);
    if (nPropertyMap) pushCount(nPropertyMap);
    if (nProperty) pushCount(nProperty);
    if (nMethodSemantics) pushCount(nMethodSemantics);
    if (nTypeSpec) pushCount(nTypeSpec);
    if (nAssembly) pushCount(nAssembly);
    if (nAssemblyRef) pushCount(nAssemblyRef);
    if (nNestedClass) pushCount(nNestedClass);
    if (nGenericParam) pushCount(nGenericParam);
    if (nMethodSpec) pushCount(nMethodSpec);
    if (nGenericParamConstraint) pushCount(nGenericParamConstraint);

    std::map<uint32_t, uint32_t> fieldList, methodList, eventMapList, propertyMapList;
    uint32_t f = 1, m = 1, e = 1, p = 1;
    for (size_t i = 0; i < typeDefs_.size(); i++) {
        uint32_t row = static_cast<uint32_t>(i + 1);
        fieldList[row] = f;
        methodList[row] = m;
        if (typeDefs_[i].eventCount > 0) {
            eventMapList[row] = e;
        }
        if (typeDefs_[i].propertyCount > 0) {
            propertyMapList[row] = p;
        }
        f += static_cast<uint32_t>(typeDefs_[i].fieldCount);
        m += static_cast<uint32_t>(typeDefs_[i].methodCount);
        e += static_cast<uint32_t>(typeDefs_[i].eventCount);
        p += static_cast<uint32_t>(typeDefs_[i].propertyCount);
    }
    std::map<uint32_t, uint32_t> paramList;
    uint32_t pa = 1;
    for (size_t i = 0; i < methodDefs_.size(); i++) {
        paramList[static_cast<uint32_t>(i + 1)] = pa;
        pa += static_cast<uint32_t>(methodDefs_[i].paramCount);
    }

    {
        t.u16(0);
        writeIndex(moduleNameIdx_, strBytes);
        writeIndex(1, guidBytes);
        writeIndex(0, guidBytes);
        writeIndex(0, guidBytes);
    }
    for (const auto& r : typeRefs_) {
        writeIndex(r.scope, resolutionScopeBytes);
        writeIndex(r.name, strBytes);
        writeIndex(r.ns, strBytes);
    }
    for (size_t i = 0; i < typeDefs_.size(); i++) {
        const auto& r = typeDefs_[i];
        uint32_t row = static_cast<uint32_t>(i + 1);
        t.u32(r.flags);
        writeIndex(r.name, strBytes);
        writeIndex(r.ns, strBytes);
        writeIndex(r.extends, tdorBytes);
        writeIndex(fieldList.at(row), simpleIndexBytes(nField));
        writeIndex(methodList.at(row), simpleIndexBytes(nMethodDef));
    }
    for (const auto& r : fields_) {
        t.u16(r.flags);
        writeIndex(r.name, strBytes);
        writeIndex(r.signature, blobBytes);
    }
    for (size_t i = 0; i < methodDefs_.size(); i++) {
        const auto& r = methodDefs_[i];
        t.u32(r.rva);
        t.u16(r.implFlags);
        t.u16(r.flags);
        writeIndex(r.name, strBytes);
        writeIndex(r.signature, blobBytes);
        writeIndex(paramList.at(static_cast<uint32_t>(i + 1)), simpleIndexBytes(nParam));
    }
    for (const auto& r : params_) {
        t.u16(r.flags);
        t.u16(r.sequence);
        writeIndex(r.name, strBytes);
    }
    {
        auto rows = interfaceImpls_;
        std::stable_sort(rows.begin(), rows.end(),
                         [](const InterfaceImplRow& a, const InterfaceImplRow& b) { return a.classRow < b.classRow; });
        for (const auto& r : rows) {
            writeIndex(r.classRow, simpleIndexBytes(nTypeDef));
            writeIndex(r.interfaceCoded, tdorBytes);
        }
    }
    for (const auto& r : memberRefs_) {
        writeIndex(r.parent, memberRefParentBytes);
        writeIndex(r.name, strBytes);
        writeIndex(r.signature, blobBytes);
    }
    {
        auto rows = constants_;
        std::stable_sort(rows.begin(), rows.end(),
                         [](const ConstantRow& a, const ConstantRow& b) { return a.parent < b.parent; });
        for (const auto& r : rows) {
            t.u8(r.type);
            t.u8(0);
            writeIndex(r.parent, hasConstantBytes);
            writeIndex(r.value, blobBytes);
        }
    }
    {
        auto rows = customAttributes_;
        std::stable_sort(rows.begin(), rows.end(), [](const CustomAttributeRow& a, const CustomAttributeRow& b) {
            return a.parent < b.parent;
        });
        for (const auto& r : rows) {
            writeIndex(r.parent, hasCustomAttributeBytes);
            writeIndex(r.ctor, customAttributeTypeBytes);
            writeIndex(r.value, blobBytes);
        }
    }
    for (const auto& r : standAloneSigs_) {
        writeIndex(r.signature, blobBytes);
    }
    for (const auto& r : eventMaps_) {
        writeIndex(r.parent, simpleIndexBytes(nTypeDef));
        auto it = eventMapList.find(r.parent);
        writeIndex(it != eventMapList.end() ? it->second : 0, simpleIndexBytes(nEvent));
    }
    for (const auto& r : events_) {
        t.u16(r.flags);
        writeIndex(r.name, strBytes);
        writeIndex(r.eventType, tdorBytes);
    }
    for (const auto& r : propertyMaps_) {
        writeIndex(r.parent, simpleIndexBytes(nTypeDef));
        auto it = propertyMapList.find(r.parent);
        writeIndex(it != propertyMapList.end() ? it->second : 0, simpleIndexBytes(nProperty));
    }
    for (const auto& r : properties_) {
        t.u16(r.flags);
        writeIndex(r.name, strBytes);
        writeIndex(r.type, blobBytes);
    }
    {
        auto rows = methodSemantics_;
        std::stable_sort(rows.begin(), rows.end(), [](const MethodSemanticsRow& a, const MethodSemanticsRow& b) {
            return a.association < b.association;
        });
        for (const auto& r : rows) {
            t.u16(r.semantics);
            writeIndex(r.method, simpleIndexBytes(nMethodDef));
            writeIndex(r.association, hasSemanticsBytes);
        }
    }
    for (const auto& r : typeSpecs_) {
        writeIndex(r.signature, blobBytes);
    }
    if (nAssembly) {
        t.u32(assemblyHashAlg_);
        t.u16(assemblyMajor_);
        t.u16(assemblyMinor_);
        t.u16(assemblyBuild_);
        t.u16(assemblyRevision_);
        t.u32(assemblyFlags_);
        writeIndex(assemblyPublicKey_, blobBytes);
        writeIndex(asmName_, strBytes);
        writeIndex(asmCulture_, strBytes);
    }
    for (const auto& r : assemblyRefs_) {
        t.u16(r.major);
        t.u16(r.minor);
        t.u16(r.build);
        t.u16(r.revision);
        t.u32(r.flags);
        writeIndex(r.publicKeyOrToken, blobBytes);
        writeIndex(r.name, strBytes);
        writeIndex(r.culture, strBytes);
        writeIndex(r.hashValue, blobBytes);
    }
    {
        auto rows = nestedClasses_;
        std::stable_sort(rows.begin(), rows.end(),
                         [](const NestedClassRow& a, const NestedClassRow& b) { return a.nested < b.nested; });
        for (const auto& r : rows) {
            writeIndex(r.nested, simpleIndexBytes(nTypeDef));
            writeIndex(r.enclosing, simpleIndexBytes(nTypeDef));
        }
    }
    {
        auto rows = genericParams_;
        std::stable_sort(rows.begin(), rows.end(),
                         [](const GenericParamRow& a, const GenericParamRow& b) { return a.owner < b.owner; });
        for (const auto& r : rows) {
            t.u16(r.number);
            t.u16(r.flags);
            writeIndex(r.owner, typeOrMethodDefBytes);
            writeIndex(r.name, strBytes);
        }
    }
    for (const auto& r : methodSpecs_) {
        writeIndex(r.method, methodDefOrRefBytes);
        writeIndex(r.instantiation, blobBytes);
    }
    {
        auto rows = genericParamConstraints_;
        std::stable_sort(rows.begin(), rows.end(), [](const GenericParamConstraintRow& a,
                                                      const GenericParamConstraintRow& b) { return a.owner < b.owner; });
        for (const auto& r : rows) {
            writeIndex(r.owner, simpleIndexBytes(nGenericParam));
            writeIndex(r.constraint, tdorBytes);
        }
    }

    out.swap(t.b);
}


std::vector<uint8_t> AssemblyWriter::serialize() const {
    std::vector<uint8_t> tables;
    serializeTables(tables);

    std::vector<uint8_t> strings(1, 0);
    for (const auto& s : stringList_) {
        strings.insert(strings.end(), s.begin(), s.end());
        strings.push_back(0);
    }

    std::vector<uint8_t> us(1, 0);
    for (const auto& s : userStrings_) {
        std::vector<uint8_t> tmp;
        Sig::pushCompressedU(tmp, static_cast<uint32_t>(s.size() * 2 + 1));
        us.insert(us.end(), tmp.begin(), tmp.end());
        for (char c : s) {
            us.push_back(static_cast<uint8_t>(c));
            us.push_back(0);
        }
        us.push_back(0);
    }

    std::vector<uint8_t> guids = makeMvid(moduleName_, assemblyName_);

    std::vector<uint8_t> blobs(1, 0);
    for (const auto& b : blobList_) {
        std::vector<uint8_t> tmp;
        Sig::pushCompressedU(tmp, static_cast<uint32_t>(b.size()));
        blobs.insert(blobs.end(), tmp.begin(), tmp.end());
        blobs.insert(blobs.end(), b.begin(), b.end());
    }

    ByteBuf root;
    root.u32(0x424A5342);
    root.u16(1);
    root.u16(1);
    root.u32(0);
    const char* ver = "v4.0.30319";
    root.u32(12);
    root.raw(ver, 10);
    root.u8(0);
    root.u8(0);
    root.u16(0);
    root.u16(5);

    struct Stream {
        uint32_t size;
        uint32_t offset;
        const char* name;
    };
    std::vector<Stream> streams = {
        {static_cast<uint32_t>(tables.size()), 0, "#~"},          {static_cast<uint32_t>(strings.size()), 0, "#Strings"},
        {static_cast<uint32_t>(us.size()), 0, "#US"},             {static_cast<uint32_t>(guids.size()), 0, "#GUID"},
        {static_cast<uint32_t>(blobs.size()), 0, "#Blob"},
    };

    uint32_t headerEnd = static_cast<uint32_t>(root.b.size());
    for (const auto& s : streams) {
        headerEnd += 8 + alignUp(static_cast<uint32_t>(std::strlen(s.name)) + 1, 4);
    }
    uint32_t streamOffset = alignUp(headerEnd, 4);
    for (auto& s : streams) {
        s.offset = streamOffset;
        streamOffset += alignUp(s.size, 4);
    }

    for (const auto& s : streams) {
        root.u32(s.offset);
        root.u32(s.size);
        const char* n = s.name;
        size_t len = std::strlen(n);
        root.raw(n, len);
        root.pad(alignUp(static_cast<uint32_t>(len) + 1, 4) - len);
    }

    root.pad(streams[0].offset - static_cast<uint32_t>(root.b.size()));
    root.raw(tables.data(), tables.size());
    root.pad(alignUp(static_cast<uint32_t>(tables.size()), 4) - static_cast<uint32_t>(tables.size()));
    root.raw(strings.data(), strings.size());
    root.pad(alignUp(static_cast<uint32_t>(strings.size()), 4) - static_cast<uint32_t>(strings.size()));
    root.raw(us.data(), us.size());
    root.pad(alignUp(static_cast<uint32_t>(us.size()), 4) - static_cast<uint32_t>(us.size()));
    root.raw(guids.data(), guids.size());
    root.pad(alignUp(static_cast<uint32_t>(guids.size()), 4) - static_cast<uint32_t>(guids.size()));
    root.raw(blobs.data(), blobs.size());
    root.pad(alignUp(static_cast<uint32_t>(blobs.size()), 4) - static_cast<uint32_t>(blobs.size()));

    return std::move(root.b);
}


std::vector<uint8_t> AssemblyWriter::write() {
    const uint32_t sectionAlign = 0x2000;
    const uint32_t fileAlign = 0x200;
    const uint32_t textRva = 0x2000;
    const uint32_t clrHeaderSize = 0x48;

    std::map<uint32_t, uint32_t> bodyLocalsSigRow;
    for (const auto& entry : bodies_) {
        if (entry.second.localsSigBlob != 0) {
            bodyLocalsSigRow[entry.first] = addStandAloneSig(entry.second.localsSigBlob);
        }
    }
    std::vector<std::vector<uint8_t>> bodies(methodDefs_.size());
    uint32_t textEnd = clrHeaderSize;
    for (size_t i = 0; i < methodDefs_.size(); i++) {
        std::vector<uint8_t> body;
        auto it = bodies_.find(static_cast<uint32_t>(i + 1));
        if (it != bodies_.end()) {
            const auto& il = it->second.il;
            auto localsIt = bodyLocalsSigRow.find(static_cast<uint32_t>(i + 1));
            bool hasLocals = localsIt != bodyLocalsSigRow.end();
            if (il.size() < 64 && !hasLocals) {
                body.push_back(static_cast<uint8_t>(0x02 | (il.size() << 2)));
            } else {
                body.push_back(0x13);
                body.push_back(0x30);
                body.push_back(8);
                body.push_back(0);
                body.push_back(static_cast<uint8_t>(il.size() & 0xFF));
                body.push_back(static_cast<uint8_t>((il.size() >> 8) & 0xFF));
                body.push_back(static_cast<uint8_t>((il.size() >> 16) & 0xFF));
                body.push_back(static_cast<uint8_t>((il.size() >> 24) & 0xFF));
                uint32_t localsTok = hasLocals ? (TokenType::StandAloneSig | localsIt->second) : 0;
                body.push_back(static_cast<uint8_t>(localsTok & 0xFF));
                body.push_back(static_cast<uint8_t>((localsTok >> 8) & 0xFF));
                body.push_back(static_cast<uint8_t>((localsTok >> 16) & 0xFF));
                body.push_back(static_cast<uint8_t>((localsTok >> 24) & 0xFF));
            }
            body.insert(body.end(), il.begin(), il.end());
        }
        if (!body.empty()) {
            methodDefs_[i].rva = textRva + textEnd;
        }
        bodies[i] = std::move(body);
        textEnd += alignUp(static_cast<uint32_t>(bodies[i].size()), 4);
    }
    uint32_t textSize = alignUp(textEnd, 4);

    std::vector<uint8_t> meta = serialize();
    uint32_t rdataRva = textRva + alignUp(textSize, sectionAlign);
    uint32_t rdataRawSize = alignUp(static_cast<uint32_t>(meta.size()), fileAlign);
    uint32_t imageSize = rdataRva + alignUp(static_cast<uint32_t>(meta.size()), sectionAlign);

    ByteBuf out;
    out.pad(0x80);
    out.b[0] = 'M';
    out.b[1] = 'Z';
    out.u32At(0x3C, 0x80);
    out.raw("PE\0\0", 4);
    out.u16(0x14C);
    out.u16(2);
    out.u32(0);
    out.u32(0);
    out.u32(0);
    out.u16(0xE0);
    out.u16(0x2102);
    out.u16(0x10B);
    out.u8(6);
    out.u8(0);
    out.u32(textSize);
    out.u32(rdataRawSize);
    out.u32(0);
    out.u32(0);
    out.u32(textRva);
    out.u32(rdataRva);
    out.u32(0x00400000);
    out.u32(sectionAlign);
    out.u32(fileAlign);
    out.u16(4);
    out.u16(0);
    out.u16(0);
    out.u16(0);
    out.u16(4);
    out.u16(0);
    out.u32(0);
    out.u32(imageSize);
    out.u32(0x200);
    out.u32(0);
    out.u16(2);
    out.u16(0x8160);
    out.u32(0x100000);
    out.u32(0x1000);
    out.u32(0x100000);
    out.u32(0x1000);
    out.u32(0);
    out.u32(16);
    for (int i = 0; i < 16; i++) {
        if (i == 14) {
            out.u32(textRva);
            out.u32(clrHeaderSize);
        } else {
            out.u32(0);
            out.u32(0);
        }
    }
    uint32_t textRawPtr = 0x200;
    uint32_t rdataRawPtr = textRawPtr + alignUp(textSize, fileAlign);
    out.raw(".text\0\0\0", 8);
    out.u32(textSize);
    out.u32(textRva);
    out.u32(alignUp(textSize, fileAlign));
    out.u32(textRawPtr);
    out.u32(0);
    out.u32(0);
    out.u16(0);
    out.u16(0);
    out.u32(0x60000020);
    out.raw(".rdata\0\0\0", 8);
    out.u32(static_cast<uint32_t>(meta.size()));
    out.u32(rdataRva);
    out.u32(rdataRawSize);
    out.u32(rdataRawPtr);
    out.u32(0);
    out.u32(0);
    out.u16(0);
    out.u16(0);
    out.u32(0x40000040);

    out.pad(textRawPtr - static_cast<uint32_t>(out.b.size()));
    out.u32(clrHeaderSize);
    out.u16(2);
    out.u16(5);
    out.u32(rdataRva);
    out.u32(static_cast<uint32_t>(meta.size()));
    out.u32(0x1);
    out.u32(0);
    out.u64(0);
    out.u64(0);
    out.u64(0);
    out.u64(0);
    out.u64(0);
    out.u64(0);
    for (const auto& body : bodies) {
        out.raw(body.data(), body.size());
        out.pad(alignUp(static_cast<uint32_t>(body.size()), 4) - static_cast<uint32_t>(body.size()));
    }

    out.pad(rdataRawPtr - static_cast<uint32_t>(out.b.size()));
    out.raw(meta.data(), meta.size());
    out.pad(rdataRawSize - static_cast<uint32_t>(meta.size()));

    return std::move(out.b);
}


void Sig::pushCompressedU(std::vector<uint8_t>& out, uint32_t v) {
    if (v < 0x80) {
        out.push_back(static_cast<uint8_t>(v));
    } else if (v < 0x4000) {
        out.push_back(static_cast<uint8_t>((v >> 8) | 0x80));
        out.push_back(static_cast<uint8_t>(v & 0xFF));
    } else {
        out.push_back(static_cast<uint8_t>((v >> 24) | 0xC0));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(v & 0xFF));
    }
}

void Sig::pushCompressedI(std::vector<uint8_t>& out, int32_t v) {
    uint32_t u = (static_cast<uint32_t>(v) << 1) ^ (static_cast<uint32_t>(v) >> 31);
    pushCompressedU(out, u);
}

void Sig::pushSerString(std::vector<uint8_t>& out, const std::string& s) {
    pushCompressedU(out, static_cast<uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

}
