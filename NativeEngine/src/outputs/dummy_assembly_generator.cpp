#include "outputs/dummy_assembly_generator.h"

#include "utils/custom_attribute_data_reader.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace il2cppdumper {

namespace {

namespace Tag {
constexpr uint32_t TypeDefOrRef_TypeDef = 0;
constexpr uint32_t TypeDefOrRef_TypeRef = 1;
constexpr uint32_t HasConstant_Field = 0;
constexpr uint32_t HasConstant_Param = 1;
constexpr uint32_t HasCustomAttribute_MethodDef = 0;
constexpr uint32_t HasCustomAttribute_Field = 1;
constexpr uint32_t HasCustomAttribute_TypeDef = 3;
constexpr uint32_t HasCustomAttribute_Param = 4;
constexpr uint32_t HasCustomAttribute_Property = 9;
constexpr uint32_t HasCustomAttribute_Event = 10;
constexpr uint32_t MemberRefParent_TypeRef = 1;
constexpr uint32_t HasSemantics_Event = 0;
constexpr uint32_t HasSemantics_Property = 1;
constexpr uint32_t ResolutionScope_AssemblyRef = 2;
constexpr uint32_t ResolutionScope_TypeRef = 3;
constexpr uint32_t CustomAttributeType_MethodDef = 2;
constexpr uint32_t CustomAttributeType_MemberRef = 3;
}

std::string hexStr(uint64_t v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(v));
    return buf;
}

void pushU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>(v >> 8));
}

void pushU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
}

void pushU64(std::vector<uint8_t>& out, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
}

struct Ctx {
    AssemblyWriter writer;
    int imageIndex = -1;
    int typeStart = 0;
    int typeEnd = 0;
    std::string imageName;

    std::unordered_map<int, uint32_t> typeRow;
    std::unordered_map<int, uint32_t> fieldRow;
    std::unordered_map<int, uint32_t> methodRow;
    std::unordered_map<int, uint32_t> paramRow;
    std::unordered_map<int, uint32_t> propertyRow;
    std::unordered_map<int, uint32_t> eventRow;
    std::unordered_map<int, uint32_t> genericParamRow;
    std::unordered_map<int, uint32_t> typeRefRow;
    std::unordered_map<int, uint32_t> assemblyRefRow;
    std::unordered_map<std::string, uint32_t> markerCtorRefs;
    uint32_t markerAssemblyRef = 0;
};

}

struct DummyAssemblyGenerator::Impl {
    Il2CppExecutor& executor;
    Metadata& metadata;
    Il2Cpp& il2Cpp;
    bool addToken;
    std::vector<int> typeImage;
    std::vector<Output> assemblies;

    Impl(Il2CppExecutor& executor_, bool addToken_) : executor(executor_), metadata(*executor.metadata),
                                                      il2Cpp(*executor.il2Cpp), addToken(addToken_) {
        typeImage.assign(metadata.typeDefs.size(), -1);
        for (size_t i = 0; i < metadata.imageDefs.size(); i++) {
            const auto& imageDef = metadata.imageDefs[i];
            for (int j = imageDef.typeStart; j < imageDef.typeStart + static_cast<int>(imageDef.typeCount) &&
                                                j < static_cast<int>(metadata.typeDefs.size());
                 j++) {
                typeImage[static_cast<size_t>(j)] = static_cast<int>(i);
            }
        }
    }

    int findTypeDef(const std::string& ns, const std::string& name) const {
        for (size_t i = 0; i < metadata.typeDefs.size(); i++) {
            const auto& td = metadata.typeDefs[i];
            if (metadata.getStringFromIndex(td.nameIndex) == name &&
                metadata.getStringFromIndex(td.namespaceIndex) == ns) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int typeDefIndexOf(const Il2CppType* t) const {
        const Il2CppTypeDefinition* td;
        if (t->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST)) {
            auto genericClass = il2Cpp.mapVATRClass<Il2CppGenericClass>(t->genericClassPtr());
            td = executor.getGenericClassTypeDefinition(&genericClass);
        } else {
            td = executor.getTypeDefinitionFromIl2CppType(t);
        }
        if (td == nullptr) {
            throw std::runtime_error("typeDefIndexOf: no type definition for type");
        }
        return static_cast<int>(td - metadata.typeDefs.data());
    }

    bool inCurrentImage(const Ctx& c, int typeDefIndex) const {
        return typeDefIndex >= c.typeStart && typeDefIndex < c.typeEnd;
    }


    uint32_t assemblyRefForImage(Ctx& c, int imageIndex) {
        auto it = c.assemblyRefRow.find(imageIndex);
        if (it != c.assemblyRefRow.end()) {
            return it->second;
        }
        const auto& imageDef = metadata.imageDefs[static_cast<size_t>(imageIndex)];
        const auto& aname = metadata.assemblyDefs[static_cast<size_t>(imageDef.assemblyIndex)].aname;
        uint16_t major, minor, build, revision;
        if (aname.build >= 0) {
            major = static_cast<uint16_t>(aname.major);
            minor = static_cast<uint16_t>(aname.minor);
            build = static_cast<uint16_t>(aname.build);
            revision = static_cast<uint16_t>(aname.revision);
        } else {
            major = 3;
            minor = 7;
            build = 1;
            revision = 6;
        }
        std::string name = metadata.getStringFromIndex(aname.nameIndex);
        std::string culture = metadata.getStringFromIndex(aname.cultureIndex);
        std::vector<uint8_t> pkt(aname.public_key_token, aname.public_key_token + 8);
        bool hasToken = false;
        if (name == "mscorlib") {
            for (uint8_t b : pkt) {
                if (b != 0) {
                    hasToken = true;
                    break;
                }
            }
        }
        uint32_t tokenBlob = hasToken ? c.writer.blobIndex(pkt) : 0;
        uint32_t row = c.writer.addAssemblyRef(major, minor, build, revision, 0, tokenBlob, name, culture, 0);
        c.assemblyRefRow[imageIndex] = row;
        return row;
    }

    uint32_t typeRefForTypeDef(Ctx& c, int typeDefIndex) {
        auto it = c.typeRefRow.find(typeDefIndex);
        if (it != c.typeRefRow.end()) {
            return it->second;
        }
        const auto& td = metadata.typeDefs[static_cast<size_t>(typeDefIndex)];
        std::string ns = metadata.getStringFromIndex(td.namespaceIndex);
        std::string name = metadata.getStringFromIndex(td.nameIndex);
        uint32_t scope;
        if (td.declaringTypeIndex != -1) {
            int declaringTypeDef = -1;
            try {
                if (td.declaringTypeIndex >= 0 &&
                    static_cast<size_t>(td.declaringTypeIndex) < il2Cpp.types.size()) {
                    const auto& declType = il2Cpp.types[static_cast<size_t>(td.declaringTypeIndex)];
                    declaringTypeDef = typeDefIndexOf(&declType);
                }
            } catch (const std::exception&) {
                declaringTypeDef = -1;
            }
            if (declaringTypeDef >= 0) {
                scope = (typeRefForTypeDef(c, declaringTypeDef) << 2) | Tag::ResolutionScope_TypeRef;
            } else {
                int img = typeImage[static_cast<size_t>(typeDefIndex)];
                if (img < 0) {
                    img = c.imageIndex;
                }
                scope = (assemblyRefForImage(c, img) << 2) | Tag::ResolutionScope_AssemblyRef;
            }
        } else {
            int img = typeImage[static_cast<size_t>(typeDefIndex)];
            if (img < 0) {
                img = c.imageIndex;
            }
            scope = (assemblyRefForImage(c, img) << 2) | Tag::ResolutionScope_AssemblyRef;
        }
        uint32_t row = c.writer.addTypeRef(scope, ns, name);
        c.typeRefRow[typeDefIndex] = row;
        return row;
    }

    uint32_t codedTypeDefOrRef(Ctx& c, int typeDefIndex) {
        if (inCurrentImage(c, typeDefIndex)) {
            auto it = c.typeRow.find(typeDefIndex);
            if (it != c.typeRow.end()) {
                return (it->second << 2) | Tag::TypeDefOrRef_TypeDef;
            }
        }
        return (typeRefForTypeDef(c, typeDefIndex) << 2) | Tag::TypeDefOrRef_TypeRef;
    }

    uint32_t tokenOfTypeDefOrRef(Ctx& c, int typeDefIndex) {
        if (inCurrentImage(c, typeDefIndex)) {
            auto it = c.typeRow.find(typeDefIndex);
            if (it != c.typeRow.end()) {
                return TokenType::TypeDef | it->second;
            }
        }
        return TokenType::TypeRef | typeRefForTypeDef(c, typeDefIndex);
    }


    std::vector<uint8_t> typeBytes(Ctx& c, const Il2CppType* t) {
        std::vector<uint8_t> out;
        switch (t->type) {
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VOID):
                out.push_back(0x01);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN):
                out.push_back(0x02);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CHAR):
                out.push_back(0x03);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I1):
                out.push_back(0x04);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U1):
                out.push_back(0x05);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I2):
                out.push_back(0x06);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U2):
                out.push_back(0x07);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I4):
                out.push_back(0x08);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U4):
                out.push_back(0x09);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I8):
                out.push_back(0x0A);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U8):
                out.push_back(0x0B);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R4):
                out.push_back(0x0C);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R8):
                out.push_back(0x0D);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
                out.push_back(0x0E);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_TYPEDBYREF):
                out.push_back(0x16);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I):
                out.push_back(0x18);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U):
                out.push_back(0x19);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT):
                out.push_back(0x1C);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE):
                out.push_back(t->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE) ? 0x11 : 0x12);
                Sig::pushCompressedU(out, codedTypeDefOrRef(c, typeDefIndexOf(t)));
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST): {
                auto genericClass = il2Cpp.mapVATRClass<Il2CppGenericClass>(t->genericClassPtr());
                const auto* td = executor.getGenericClassTypeDefinition(&genericClass);
                if (td == nullptr) {
                    throw std::runtime_error("generic class type definition not found");
                }
                out.push_back(0x15);
                out.push_back(td->isValueType() ? 0x11 : 0x12);
                Sig::pushCompressedU(out, codedTypeDefOrRef(c, static_cast<int>(td - metadata.typeDefs.data())));
                auto genericInst = il2Cpp.mapVATRClass<Il2CppGenericInst>(genericClass.context.class_inst);
                auto pointers = il2Cpp.mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                Sig::pushCompressedU(out, static_cast<uint32_t>(pointers.size()));
                for (uint64_t p : pointers) {
                    const Il2CppType* arg = il2Cpp.getIl2CppType(p);
                    auto ab = typeBytes(c, arg);
                    out.insert(out.end(), ab.begin(), ab.end());
                }
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_ARRAY): {
                auto arrayType = il2Cpp.mapVATRClass<Il2CppArrayType>(t->arrayPtr());
                const Il2CppType* elem = il2Cpp.getIl2CppType(arrayType.etype);
                out.push_back(0x14);
                auto eb = typeBytes(c, elem);
                out.insert(out.end(), eb.begin(), eb.end());
                out.push_back(arrayType.rank);
                out.push_back(0);
                out.push_back(0);
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
                const Il2CppType* elem = il2Cpp.getIl2CppType(t->typePtr());
                out.push_back(0x1D);
                auto eb = typeBytes(c, elem);
                out.insert(out.end(), eb.begin(), eb.end());
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR): {
                const Il2CppType* elem = il2Cpp.getIl2CppType(t->typePtr());
                out.push_back(0x0F);
                auto eb = typeBytes(c, elem);
                out.insert(out.end(), eb.begin(), eb.end());
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VAR):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_MVAR): {
                const auto* param = executor.getGenericParameteFromIl2CppType(t);
                out.push_back(t->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VAR) ? 0x13 : 0x1E);
                Sig::pushCompressedU(out, param->num);
                break;
            }
            default:
                throw std::runtime_error("typeBytes: unsupported type");
        }
        return out;
    }

    std::vector<uint8_t> typeBytesWithByRef(Ctx& c, const Il2CppType* t) {
        std::vector<uint8_t> out;
        if (t->byref == 1) {
            out.push_back(0x10);
            auto inner = typeBytes(c, t);
            out.insert(out.end(), inner.begin(), inner.end());
            return out;
        }
        return typeBytes(c, t);
    }


    uint32_t markerCtorRef(Ctx& c, const std::string& attrName) {
        auto it = c.markerCtorRefs.find(attrName);
        if (it != c.markerCtorRefs.end()) {
            return it->second;
        }
        if (c.markerAssemblyRef == 0) {
            c.markerAssemblyRef = c.writer.addAssemblyRef(1, 0, 0, 0, 0, 0, "Il2CppDummyDll", "", 0);
        }
        uint32_t scope = (c.markerAssemblyRef << 2) | Tag::ResolutionScope_AssemblyRef;
        uint32_t typeRef = c.writer.addTypeRef(scope, "Il2CppDummyDll", attrName);
        std::vector<uint8_t> voidRet = {0x01};
        uint32_t sig = c.writer.methodSig(true, false, 0, voidRet, {});
        uint32_t row = c.writer.addMemberRef((typeRef << 3) | Tag::MemberRefParent_TypeRef, ".ctor", sig);
        uint32_t coded = (row << 3) | Tag::CustomAttributeType_MemberRef;
        c.markerCtorRefs[attrName] = coded;
        return coded;
    }

    static std::vector<uint8_t> markerBlob(const std::vector<std::pair<std::string, std::string>>& fields) {
        std::vector<uint8_t> b = {0x01, 0x00};
        pushU16(b, static_cast<uint16_t>(fields.size()));
        for (const auto& f : fields) {
            b.push_back(0x53);
            b.push_back(0x0E);
            Sig::pushSerString(b, f.first);
            Sig::pushSerString(b, f.second);
        }
        return b;
    }

    void addMarkerAttribute(Ctx& c, const std::string& attrName,
                            const std::vector<std::pair<std::string, std::string>>& fields, uint32_t parentCoded) {
        c.writer.addCustomAttribute(parentCoded, markerCtorRef(c, attrName), c.writer.blobIndex(markerBlob(fields)));
    }

    void addTokenAttribute(Ctx& c, uint32_t parentCoded, uint32_t token) {
        addMarkerAttribute(c, "TokenAttribute", {{"Token", hexStr(token)}}, parentCoded);
    }


    void addConstantValue(Ctx& c, const BlobValue& v, uint32_t parentCoded) {
        std::vector<uint8_t> bytes;
        uint8_t type = 0;
        switch (v.kind) {
            case BlobValue::Kind::Bool:
                type = 0x02;
                bytes.push_back(v.b ? 1 : 0);
                break;
            case BlobValue::Kind::Char:
                type = 0x03;
                pushU16(bytes, static_cast<uint16_t>(v.ch));
                break;
            case BlobValue::Kind::I8:
                type = 0x04;
                bytes.push_back(static_cast<uint8_t>(v.i8));
                break;
            case BlobValue::Kind::U8:
                type = 0x05;
                bytes.push_back(v.u8);
                break;
            case BlobValue::Kind::I16:
                type = 0x06;
                pushU16(bytes, static_cast<uint16_t>(v.i16));
                break;
            case BlobValue::Kind::U16:
                type = 0x07;
                pushU16(bytes, v.u16);
                break;
            case BlobValue::Kind::I32:
                type = 0x08;
                pushU32(bytes, static_cast<uint32_t>(v.i32));
                break;
            case BlobValue::Kind::U32:
                type = 0x09;
                pushU32(bytes, v.u32);
                break;
            case BlobValue::Kind::I64:
                type = 0x0A;
                pushU64(bytes, static_cast<uint64_t>(v.i64));
                break;
            case BlobValue::Kind::U64:
                type = 0x0B;
                pushU64(bytes, v.u64);
                break;
            case BlobValue::Kind::F32: {
                type = 0x0C;
                uint32_t bits;
                std::memcpy(&bits, &v.f32, 4);
                pushU32(bytes, bits);
                break;
            }
            case BlobValue::Kind::F64: {
                type = 0x0D;
                uint64_t bits;
                std::memcpy(&bits, &v.f64, 8);
                pushU64(bytes, bits);
                break;
            }
            case BlobValue::Kind::String:
                type = 0x0E;
                bytes.insert(bytes.end(), v.str.begin(), v.str.end());
                break;
            default:
                return;
        }
        c.writer.addConstant(type, parentCoded, c.writer.blobIndex(bytes));
    }


    bool hasBody(const Il2CppMethodDefinition& md) const {
        return !(md.flags & 0x0400) && !(md.flags & 0x2000) && ((md.iflags & 0x3) != 0x3);
    }

    bool isMulticastDelegate(const Il2CppTypeDefinition& td) const {
        if (td.parentIndex < 0) {
            return false;
        }
        try {
            const auto* pt = &il2Cpp.types[static_cast<size_t>(td.parentIndex)];
            const auto* ptd = executor.getTypeDefinitionFromIl2CppType(pt);
            return metadata.getStringFromIndex(ptd->nameIndex) == "MulticastDelegate" &&
                   metadata.getStringFromIndex(ptd->namespaceIndex) == "System";
        } catch (...) {
            return false;
        }
    }

    bool isValueTypeType(const Il2CppType* t) const {
        if (t->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE)) {
            return true;
        }
        if (t->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST)) {
            try {
                auto genericClass = il2Cpp.mapVATRClass<Il2CppGenericClass>(t->genericClassPtr());
                const auto* td = executor.getGenericClassTypeDefinition(&genericClass);
                return td != nullptr && td->isValueType();
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    uint32_t tokenOfType(Ctx& c, const Il2CppType* t) {
        if (t->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST)) {
            auto genericClass = il2Cpp.mapVATRClass<Il2CppGenericClass>(t->genericClassPtr());
            const auto* td = executor.getGenericClassTypeDefinition(&genericClass);
            return tokenOfTypeDefOrRef(c, static_cast<int>(td - metadata.typeDefs.data()));
        }
        return tokenOfTypeDefOrRef(c, typeDefIndexOf(t));
    }

    void buildBody(Ctx& c, const Il2CppType* returnType, uint32_t methodRow) {
        std::vector<uint8_t> il;
        if (returnType->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VOID)) {
            il.push_back(0x2A);
            c.writer.setMethodBody(methodRow, il);
        } else if (isValueTypeType(returnType)) {
            il.push_back(0x11);
            il.push_back(0x00);
            il.push_back(0xFE);
            il.push_back(0x15);
            uint32_t tok = tokenOfType(c, returnType);
            pushU32(il, tok);
            il.push_back(0x06);
            il.push_back(0x2A);
            std::vector<uint8_t> localsSig;
            localsSig.push_back(0x07);
            Sig::pushCompressedU(localsSig, 1);
            auto tb = typeBytes(c, returnType);
            localsSig.insert(localsSig.end(), tb.begin(), tb.end());
            c.writer.setMethodBody(methodRow, il, c.writer.blobIndex(localsSig));
        } else {
            il.push_back(0x14);
            il.push_back(0x2A);
            c.writer.setMethodBody(methodRow, il);
        }
    }


    const Il2CppType* propertyType(const Il2CppTypeDefinition& typeDef, const Il2CppPropertyDefinition& pDef) {
        int typeIndex = static_cast<int>(&typeDef - metadata.typeDefs.data());
        if (pDef.get >= 0) {
            const auto& m = metadata.methodDefs[static_cast<size_t>(metadata.getMethodIndexFromTypeDefinition(typeIndex, pDef.get))];
            return &il2Cpp.types[static_cast<size_t>(m.returnType)];
        }
        if (pDef.set >= 0) {
            const auto& m = metadata.methodDefs[static_cast<size_t>(metadata.getMethodIndexFromTypeDefinition(typeIndex, pDef.set))];
            const auto& p = metadata.parameterDefs[static_cast<size_t>(m.parameterStart)];
            return &il2Cpp.types[static_cast<size_t>(p.typeIndex)];
        }
        return nullptr;
    }


    uint32_t addGenericParamRow(Ctx& c, int gpIndex, uint32_t ownerCoded) {
        auto it = c.genericParamRow.find(gpIndex);
        if (it != c.genericParamRow.end()) {
            return it->second;
        }
        const auto& param = metadata.genericParameters[static_cast<size_t>(gpIndex)];
        uint32_t row =
            c.writer.addGenericParam(param.num, param.flags, ownerCoded, metadata.getStringFromIndex(param.nameIndex));
        c.genericParamRow[gpIndex] = row;
        for (int j = 0; j < param.constraintsCount; j++) {
            int tIdx = metadata.constraintIndices[static_cast<size_t>(param.constraintsStart + j)];
            const auto* ct = &il2Cpp.types[static_cast<size_t>(tIdx)];
            try {
                c.writer.addGenericParamConstraint(row, codedTypeDefOrRef(c, typeDefIndexOf(ct)));
            } catch (...) {
            }
        }
        return row;
    }


    void createCustomAttribute(Ctx& c, int imageIndex, int customAttributeIndex, uint32_t token,
                               uint32_t parentCoded) {
        int attributeIndex = metadata.getCustomAttributeIndex(imageIndex, customAttributeIndex, token);
        if (attributeIndex < 0) {
            return;
        }
        try {
            if (il2Cpp.version < 29) {
                const auto& range = metadata.attributeTypeRanges[static_cast<size_t>(attributeIndex)];
                for (int i = 0; i < range.count; i++) {
                    int typeIndex = metadata.attributeTypes[static_cast<size_t>(range.start + i)];
                    const auto& attrType = il2Cpp.types[static_cast<size_t>(typeIndex)];
                    int attrTypeDefIndex = typeDefIndexOf(&attrType);
                    if (!tryRestoreCustomAttribute(c, attrTypeDefIndex, parentCoded)) {
                        uint64_t methodPointer = executor.customAttributeGenerators[static_cast<size_t>(attributeIndex)];
                        uint64_t fixedPointer = il2Cpp.getRVA(methodPointer);
                        const auto& td = metadata.typeDefs[static_cast<size_t>(attrTypeDefIndex)];
                        addMarkerAttribute(c, "AttributeAttribute",
                                           {{"Name", metadata.getStringFromIndex(td.nameIndex)},
                                            {"RVA", hexStr(fixedPointer)},
                                            {"Offset", hexStr(il2Cpp.mapVATR(methodPointer))}},
                                           parentCoded);
                    }
                }
            } else {
                const auto& startRange = metadata.attributeDataRanges[static_cast<size_t>(attributeIndex)];
                const auto& endRange = metadata.attributeDataRanges[static_cast<size_t>(attributeIndex + 1)];
                metadata.setPosition(static_cast<size_t>(metadata.header.attributeDataOffset) + startRange.startOffset);
                auto buff = metadata.readBytes(endRange.startOffset - startRange.startOffset);
                CustomAttributeDataReader reader(executor, std::move(buff));
                for (uint32_t i = 0; i < reader.count(); i++) {
                    auto visitor = reader.visitCustomAttributeData();
                    if (visitor.ctorIndex < 0 ||
                        static_cast<size_t>(visitor.ctorIndex) >= metadata.methodDefs.size()) {
                        throw std::runtime_error("attribute ctor index out of range");
                    }
                    const auto& ctorDef = metadata.methodDefs[static_cast<size_t>(visitor.ctorIndex)];
                    auto ctorIt = c.methodRow.find(visitor.ctorIndex);
                    uint32_t ctorCoded;
                    if (ctorIt != c.methodRow.end()) {
                        ctorCoded = (ctorIt->second << 3) | Tag::CustomAttributeType_MethodDef;
                    } else {
                        int attrTypeDef = ctorDef.declaringType;
                        uint32_t typeRef = typeRefForTypeDef(c, attrTypeDef);
                        std::vector<std::vector<uint8_t>> paramTypes;
                        for (int k = 0; k < ctorDef.parameterCount; k++) {
                            const auto& pd =
                                metadata.parameterDefs[static_cast<size_t>(ctorDef.parameterStart + k)];
                            paramTypes.push_back(
                                typeBytesWithByRef(c, &il2Cpp.types[static_cast<size_t>(pd.typeIndex)]));
                        }
                        std::vector<uint8_t> voidRet = {0x01};
                        uint32_t sig = c.writer.methodSig(true, false, 0, voidRet, paramTypes);
                        uint32_t memberRef =
                            c.writer.addMemberRef((typeRef << 3) | Tag::MemberRefParent_TypeRef, ".ctor", sig);
                        ctorCoded = (memberRef << 3) | Tag::CustomAttributeType_MemberRef;
                    }
                    std::vector<uint8_t> blob;
                    blob.push_back(0x01);
                    blob.push_back(0x00);
                    for (const auto& arg : visitor.arguments) {
                        if (arg.index < 0 || arg.index >= ctorDef.parameterCount) {
                            continue;
                        }
                        const auto& paramDef =
                            metadata.parameterDefs[static_cast<size_t>(ctorDef.parameterStart + arg.index)];
                        const auto* ptype = &il2Cpp.types[static_cast<size_t>(paramDef.typeIndex)];
                        writeFixedArg(blob, ptype, arg.value);
                    }
                    uint16_t namedCount = 0;
                    for (const auto& f : visitor.fields) {
                        if (c.fieldRow.count(f.index) != 0) {
                            namedCount++;
                        }
                    }
                    for (const auto& pr : visitor.properties) {
                        if (c.propertyRow.count(pr.index) != 0) {
                            namedCount++;
                        }
                    }
                    pushU16(blob, namedCount);
                    for (const auto& f : visitor.fields) {
                        auto fit = c.fieldRow.find(f.index);
                        if (fit == c.fieldRow.end()) {
                            continue;
                        }
                        const auto& fDef = metadata.fieldDefs[static_cast<size_t>(f.index)];
                        const auto* ftype = &il2Cpp.types[static_cast<size_t>(fDef.typeIndex)];
                        blob.push_back(0x53);
                        writeFieldOrPropType(c, blob, ftype);
                        Sig::pushSerString(blob, metadata.getStringFromIndex(fDef.nameIndex));
                        writeAttrValue(blob, ftype, f.value);
                    }
                    for (const auto& pr : visitor.properties) {
                        auto pit = c.propertyRow.find(pr.index);
                        if (pit == c.propertyRow.end()) {
                            continue;
                        }
                        const auto& pDef = metadata.propertyDefs[static_cast<size_t>(pr.index)];
                        int ownerType = -1;
                        for (size_t ti = 0; ti < metadata.typeDefs.size(); ti++) {
                            const auto& tdd = metadata.typeDefs[ti];
                            if (pr.index >= tdd.propertyStart &&
                                pr.index < tdd.propertyStart + tdd.property_count) {
                                ownerType = static_cast<int>(ti);
                                break;
                            }
                        }
                        if (ownerType < 0) {
                            continue;
                        }
                        const auto& td = metadata.typeDefs[static_cast<size_t>(ownerType)];
                        const auto* ptype = propertyType(td, pDef);
                        if (ptype == nullptr) {
                            continue;
                        }
                        blob.push_back(0x54);
                        writeFieldOrPropType(c, blob, ptype);
                        Sig::pushSerString(blob, metadata.getStringFromIndex(pDef.nameIndex));
                        writeAttrValue(blob, ptype, pr.value);
                    }
                    c.writer.addCustomAttribute(parentCoded, ctorCoded, c.writer.blobIndex(blob));
                }
            }
        } catch (const std::exception& e) {
            std::cout << "ERROR: Error while restoring attributeIndex " << attributeIndex << ": " << e.what() << "\n";
        }
    }

    bool tryRestoreCustomAttribute(Ctx& c, int attrTypeDefIndex, uint32_t parentCoded) {
        const auto& td = metadata.typeDefs[static_cast<size_t>(attrTypeDefIndex)];
        std::string typeName = metadata.getStringFromIndex(td.nameIndex);
        if (typeName == "CompilerGeneratedAttribute") {
            return false;
        }
        if (td.method_count == 1) {
            int methodIndex = metadata.getMethodIndexFromTypeDefinition(attrTypeDefIndex, 0);
            const auto& m = metadata.methodDefs[static_cast<size_t>(methodIndex)];
            if (metadata.getStringFromIndex(m.nameIndex) == ".ctor" && m.parameterCount == 0) {
                auto it = c.methodRow.find(methodIndex);
                if (it != c.methodRow.end()) {
                    c.writer.addCustomAttribute(parentCoded, (it->second << 3) | Tag::CustomAttributeType_MethodDef,
                                                c.writer.blobIndex({0x01, 0x00, 0x00, 0x00}));
                    return true;
                }
            }
        }
        return false;
    }


    std::string assemblyNameOf(int typeDefIndex) const {
        int img = typeImage[static_cast<size_t>(typeDefIndex)];
        if (img < 0) {
            return "mscorlib";
        }
        const auto& imageDef = metadata.imageDefs[static_cast<size_t>(img)];
        const auto& aname = metadata.assemblyDefs[static_cast<size_t>(imageDef.assemblyIndex)].aname;
        return metadata.getStringFromIndex(aname.nameIndex);
    }

    std::string typeFullName(const Il2CppType* t) {
        switch (t->type) {
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT):
                return "System.Object";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VOID):
                return "System.Void";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN):
                return "System.Boolean";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CHAR):
                return "System.Char";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I1):
                return "System.SByte";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U1):
                return "System.Byte";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I2):
                return "System.Int16";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U2):
                return "System.UInt16";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I4):
                return "System.Int32";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U4):
                return "System.UInt32";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I8):
                return "System.Int64";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U8):
                return "System.UInt64";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R4):
                return "System.Single";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R8):
                return "System.Double";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
                return "System.String";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I):
                return "System.IntPtr";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U):
                return "System.UIntPtr";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_TYPEDBYREF):
                return "System.TypedReference";
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
                const Il2CppType* elem = il2Cpp.getIl2CppType(t->typePtr());
                return typeFullName(elem) + "[]";
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_ARRAY): {
                auto arrayType = il2Cpp.mapVATRClass<Il2CppArrayType>(t->arrayPtr());
                const Il2CppType* elem = il2Cpp.getIl2CppType(arrayType.etype);
                std::string commas = arrayType.rank > 1 ? std::string(arrayType.rank - 1, ',') : "";
                return typeFullName(elem) + "[" + commas + "]";
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR): {
                const Il2CppType* elem = il2Cpp.getIl2CppType(t->typePtr());
                return typeFullName(elem) + "*";
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BYREF): {
                const Il2CppType* elem = il2Cpp.getIl2CppType(t->typePtr());
                return typeFullName(elem) + "&";
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VAR):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_MVAR): {
                const auto* param = executor.getGenericParameteFromIl2CppType(t);
                return metadata.getStringFromIndex(param->nameIndex);
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE): {
                int tdi = typeDefIndexOf(t);
                const auto& td = metadata.typeDefs[static_cast<size_t>(tdi)];
                std::string ns = metadata.getStringFromIndex(td.namespaceIndex);
                std::string name = metadata.getStringFromIndex(td.nameIndex);
                return ns.empty() ? name : ns + "." + name;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST): {
                auto genericClass = il2Cpp.mapVATRClass<Il2CppGenericClass>(t->genericClassPtr());
                const auto* td = executor.getGenericClassTypeDefinition(&genericClass);
                if (td == nullptr) {
                    return "System.Object";
                }
                std::string ns = metadata.getStringFromIndex(td->namespaceIndex);
                std::string name = metadata.getStringFromIndex(td->nameIndex);
                auto genericInst = il2Cpp.mapVATRClass<Il2CppGenericInst>(genericClass.context.class_inst);
                auto pointers = il2Cpp.mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                std::string args;
                for (size_t i = 0; i < pointers.size(); i++) {
                    if (i != 0) {
                        args += ",";
                    }
                    const Il2CppType* arg = il2Cpp.getIl2CppType(pointers[i]);
                    args += "[" + typeFullName(arg) + ",";
                    args += assemblyNameOf(argTypeDefIndex(arg)) + "]";
                }
                std::string full = (ns.empty() ? name : ns + "." + name) + "`" + std::to_string(pointers.size());
                if (!args.empty()) {
                    full += "[[" + args + "]]";
                }
                return full;
            }
            default:
                return "System.Object";
        }
    }

    int argTypeDefIndex(const Il2CppType* t) const {
        if (t->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS) ||
            t->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE) ||
            t->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST)) {
            try {
                return typeDefIndexOf(t);
            } catch (...) {
            }
        }
        return -1;
    }

    void writeRawByKind(std::vector<uint8_t>& out, const BlobValue& v) {
        switch (v.kind) {
            case BlobValue::Kind::Bool:
                out.push_back(v.b ? 1 : 0);
                break;
            case BlobValue::Kind::Char:
                pushU16(out, static_cast<uint16_t>(v.ch));
                break;
            case BlobValue::Kind::I8:
                out.push_back(static_cast<uint8_t>(v.i8));
                break;
            case BlobValue::Kind::U8:
                out.push_back(v.u8);
                break;
            case BlobValue::Kind::I16:
                pushU16(out, static_cast<uint16_t>(v.i16));
                break;
            case BlobValue::Kind::U16:
                pushU16(out, v.u16);
                break;
            case BlobValue::Kind::I32:
                pushU32(out, static_cast<uint32_t>(v.i32));
                break;
            case BlobValue::Kind::U32:
                pushU32(out, v.u32);
                break;
            case BlobValue::Kind::I64:
                pushU64(out, static_cast<uint64_t>(v.i64));
                break;
            case BlobValue::Kind::U64:
                pushU64(out, v.u64);
                break;
            case BlobValue::Kind::F32: {
                uint32_t bits;
                std::memcpy(&bits, &v.f32, 4);
                pushU32(out, bits);
                break;
            }
            case BlobValue::Kind::F64: {
                uint64_t bits;
                std::memcpy(&bits, &v.f64, 8);
                pushU64(out, bits);
                break;
            }
            case BlobValue::Kind::String:
                Sig::pushSerString(out, v.str);
                break;
            case BlobValue::Kind::TypeIndex: {
                const auto* t = &il2Cpp.types[static_cast<size_t>(v.typeIndex)];
                Sig::pushSerString(out, typeFullName(t));
                break;
            }
            case BlobValue::Kind::Array: {
                pushU32(out, static_cast<uint32_t>(v.array.size()));
                for (const auto& e : v.array) {
                    writeRawByKind(out, e);
                }
                break;
            }
            default:
                out.push_back(0xFF);
        }
    }

    void writeRawValue(std::vector<uint8_t>& out, const Il2CppType* t, const BlobValue& v) {
        switch (t->type) {
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN):
                out.push_back(v.b ? 1 : 0);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CHAR):
                pushU16(out, static_cast<uint16_t>(v.ch));
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I1):
                out.push_back(static_cast<uint8_t>(v.i8));
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U1):
                out.push_back(v.u8);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I2):
                pushU16(out, static_cast<uint16_t>(v.i16));
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U2):
                pushU16(out, v.u16);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I4):
                pushU32(out, static_cast<uint32_t>(v.i32));
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U4):
                pushU32(out, v.u32);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I8):
                pushU64(out, static_cast<uint64_t>(v.i64));
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U8):
                pushU64(out, v.u64);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R4): {
                uint32_t bits;
                std::memcpy(&bits, &v.f32, 4);
                pushU32(out, bits);
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R8): {
                uint64_t bits;
                std::memcpy(&bits, &v.f64, 8);
                pushU64(out, bits);
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
                if (v.kind == BlobValue::Kind::None) {
                    out.push_back(0xFF);
                } else {
                    Sig::pushSerString(out, v.str);
                }
                break;
            default:
                writeRawByKind(out, v);
        }
    }

    void writeFieldOrPropType(Ctx& c, std::vector<uint8_t>& out, const Il2CppType* t) {
        switch (t->type) {
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CHAR):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I1):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U1):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I2):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U2):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I4):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U4):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I8):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U8):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R4):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R8): {
                out.push_back(static_cast<uint8_t>(t->type));
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
                out.push_back(0x0E);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT):
                out.push_back(0x51);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
                const Il2CppType* elem = il2Cpp.getIl2CppType(t->typePtr());
                out.push_back(0x1D);
                writeFieldOrPropType(c, out, elem);
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS): {
                const auto* td = executor.getTypeDefinitionFromIl2CppType(t);
                if (td != nullptr && metadata.getStringFromIndex(td->nameIndex) == "Type" &&
                    metadata.getStringFromIndex(td->namespaceIndex) == "System") {
                    out.push_back(0x50);
                } else {
                    out.push_back(0x51);
                }
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE): {
                out.push_back(0x55);
                Sig::pushSerString(out, serializedEnumName(t));
                break;
            }
            default:
                out.push_back(0x51);
        }
    }

    std::string serializedEnumName(const Il2CppType* t) {
        int tdi = argTypeDefIndex(t);
        if (tdi >= 0) {
            return typeFullName(t) + ", " + assemblyNameOf(tdi);
        }
        return typeFullName(t);
    }

    void writeBoxedElement(std::vector<uint8_t>& out, const BlobValue& e) {
        if (e.kind == BlobValue::Kind::None) {
            out.push_back(0x0E);
            out.push_back(0xFF);
            return;
        }
        if (e.enumTypeIndex != 0) {
            out.push_back(0x55);
            const auto& et = il2Cpp.types[static_cast<size_t>(e.enumTypeIndex)];
            Sig::pushSerString(out, serializedEnumName(&et));
            writeRawByKind(out, e);
            return;
        }
        if (e.il2CppTypeEnum == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_IL2CPP_TYPE_INDEX)) {
            out.push_back(0x50);
            const auto* t = &il2Cpp.types[static_cast<size_t>(e.typeIndex)];
            Sig::pushSerString(out, typeFullName(t));
            return;
        }
        if (e.il2CppTypeEnum == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING)) {
            out.push_back(0x0E);
            writeRawByKind(out, e);
            return;
        }
        out.push_back(static_cast<uint8_t>(e.il2CppTypeEnum));
        writeRawByKind(out, e);
    }

    void writeBoxedValue(std::vector<uint8_t>& out, const BlobValue& v) {
        if (v.kind == BlobValue::Kind::None) {
            out.push_back(0x0E);
            out.push_back(0xFF);
            return;
        }
        if (v.enumTypeIndex != 0) {
            out.push_back(0x55);
            const auto& et = il2Cpp.types[static_cast<size_t>(v.enumTypeIndex)];
            Sig::pushSerString(out, serializedEnumName(&et));
            writeRawByKind(out, v);
            return;
        }
        switch (v.il2CppTypeEnum) {
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
                out.push_back(0x0E);
                Sig::pushSerString(out, v.str);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_IL2CPP_TYPE_INDEX): {
                out.push_back(0x50);
                const auto* t = &il2Cpp.types[static_cast<size_t>(v.typeIndex)];
                Sig::pushSerString(out, typeFullName(t));
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
                out.push_back(0x1D);
                out.push_back(0x51);
                pushU32(out, static_cast<uint32_t>(v.array.size()));
                for (const auto& e : v.array) {
                    writeBoxedElement(out, e);
                }
                break;
            }
            default:
                out.push_back(static_cast<uint8_t>(v.il2CppTypeEnum));
                writeRawByKind(out, v);
        }
    }

    void writeAttrValue(std::vector<uint8_t>& out, const Il2CppType* declared, const BlobValue& v) {
        switch (declared->type) {
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
                if (v.kind == BlobValue::Kind::None) {
                    out.push_back(0xFF);
                } else {
                    Sig::pushSerString(out, v.str);
                }
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT):
                writeBoxedValue(out, v);
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
                if (v.kind == BlobValue::Kind::None) {
                    pushU32(out, 0xFFFFFFFF);
                    break;
                }
                const Il2CppType* elem = il2Cpp.getIl2CppType(declared->typePtr());
                pushU32(out, static_cast<uint32_t>(v.array.size()));
                for (const auto& e : v.array) {
                    if (elem->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT)) {
                        writeBoxedValue(out, e);
                    } else {
                        writeRawValue(out, elem, e);
                    }
                }
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS): {
                const auto* td = executor.getTypeDefinitionFromIl2CppType(declared);
                if (td != nullptr && metadata.getStringFromIndex(td->nameIndex) == "Type" &&
                    metadata.getStringFromIndex(td->namespaceIndex) == "System") {
                    if (v.kind == BlobValue::Kind::None) {
                        out.push_back(0xFF);
                    } else {
                        Sig::pushSerString(out, typeFullName(&il2Cpp.types[static_cast<size_t>(v.typeIndex)]));
                    }
                } else {
                    writeBoxedValue(out, v);
                }
                break;
            }
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE):
                writeRawByKind(out, v);
                break;
            default:
                writeRawValue(out, declared, v);
        }
    }

    void writeFixedArg(std::vector<uint8_t>& out, const Il2CppType* declared, const BlobValue& v) {
        writeAttrValue(out, declared, v);
    }


    void buildMarkerAssembly() {
        Ctx c;
        c.writer = AssemblyWriter("Il2CppDummyDll", "Il2CppDummyDll.dll", 1, 0, 0, 0, il2Cpp.is32Bit);
        c.writer.setAssembly(0x8004, 1, 0, 0, 0, 0, 0, "Il2CppDummyDll", "");

        std::vector<uint8_t> mscorlibPkt = {0xB7, 0x7A, 0x5C, 0x56, 0x19, 0x34, 0xE0, 0x89};
        uint32_t mscorlibRef = c.writer.addAssemblyRef(4, 0, 0, 0, 0, c.writer.blobIndex(mscorlibPkt), "mscorlib", "", 0);
        uint32_t scope = (mscorlibRef << 2) | Tag::ResolutionScope_AssemblyRef;
        uint32_t systemAttribute = c.writer.addTypeRef(scope, "System", "Attribute");
        uint32_t attrCoded = (systemAttribute << 2) | Tag::TypeDefOrRef_TypeRef;

        std::vector<uint8_t> voidRet = {0x01};
        uint32_t ctorSig = c.writer.methodSig(true, false, 0, voidRet, {});
        uint32_t stringFieldSig = c.writer.fieldSig({0x0E});

        auto addAttrClass = [&](const std::string& name, const std::vector<std::string>& fields) {
            c.writer.addTypeDef("Il2CppDummyDll", name, 0x100001, attrCoded,
                                static_cast<int>(fields.size()), 1, 0, 0);
            for (const auto& f : fields) {
                c.writer.addField(0x6, f, stringFieldSig);
            }
            uint32_t ctor = c.writer.addMethodDef(0, 0x1886, ".ctor", ctorSig, 0);
            c.writer.setMethodBody(ctor, {0x2A});
        };

        addAttrClass("AddressAttribute", {"RVA", "Offset", "VA", "Slot"});
        addAttrClass("FieldOffsetAttribute", {"Offset"});
        addAttrClass("AttributeAttribute", {"Name", "RVA", "Offset"});
        addAttrClass("MetadataOffsetAttribute", {"Offset"});
        addAttrClass("TokenAttribute", {"Token"});

        assemblies.push_back({std::string("Il2CppDummyDll.dll"), c.writer.write()});
    }

    void buildImage(int imageIndex) {
        const auto& imageDef = metadata.imageDefs[static_cast<size_t>(imageIndex)];
        Ctx c;
        c.imageIndex = imageIndex;
        c.typeStart = imageDef.typeStart;
        c.typeEnd = imageDef.typeStart + static_cast<int>(imageDef.typeCount);
        c.imageName = metadata.getStringFromIndex(imageDef.nameIndex);

        const auto& aname = metadata.assemblyDefs[static_cast<size_t>(imageDef.assemblyIndex)].aname;
        uint16_t major, minor, build, revision;
        if (aname.build >= 0) {
            major = static_cast<uint16_t>(aname.major);
            minor = static_cast<uint16_t>(aname.minor);
            build = static_cast<uint16_t>(aname.build);
            revision = static_cast<uint16_t>(aname.revision);
        } else {
            major = 3;
            minor = 7;
            build = 1;
            revision = 6;
        }
        std::string assemblyName = metadata.getStringFromIndex(aname.nameIndex);
        c.writer = AssemblyWriter(assemblyName, c.imageName, major, minor, build, revision, il2Cpp.is32Bit);
        c.writer.setAssembly(0x8004, major, minor, build, revision, 0, 0, assemblyName, "");

        std::vector<std::vector<int>> nestedList(static_cast<size_t>(c.typeEnd - c.typeStart));
        std::vector<char> isNested(static_cast<size_t>(c.typeEnd - c.typeStart), 0);
        for (int i = c.typeStart; i < c.typeEnd; i++) {
            const auto& td = metadata.typeDefs[static_cast<size_t>(i)];
            for (int j = 0; j < td.nested_type_count; j++) {
                int nested = metadata.nestedTypeIndices[static_cast<size_t>(td.nestedTypesStart + j)];
                if (nested >= c.typeStart && nested < c.typeEnd) {
                    nestedList[static_cast<size_t>(i - c.typeStart)].push_back(nested);
                    isNested[static_cast<size_t>(nested - c.typeStart)] = 1;
                }
            }
        }
        std::vector<int> typeOrder;
        typeOrder.reserve(static_cast<size_t>(c.typeEnd - c.typeStart));
        std::vector<char> visited(static_cast<size_t>(c.typeEnd - c.typeStart), 0);
        std::function<void(int)> emitType = [&](int i) {
            size_t slot = static_cast<size_t>(i - c.typeStart);
            if (visited[slot]) {
                return;
            }
            visited[slot] = 1;
            typeOrder.push_back(i);
            for (int n : nestedList[slot]) {
                emitType(n);
            }
        };
        for (int i = c.typeStart; i < c.typeEnd; i++) {
            if (!isNested[static_cast<size_t>(i - c.typeStart)]) {
                emitType(i);
            }
        }
        for (int i = c.typeStart; i < c.typeEnd; i++) {
            emitType(i);
        }

        for (int i : typeOrder) {
            const auto& td = metadata.typeDefs[static_cast<size_t>(i)];
            uint32_t extends = 0;
            if (td.parentIndex >= 0) {
                try {
                    const auto* pt = &il2Cpp.types[static_cast<size_t>(td.parentIndex)];
                    extends = codedTypeDefOrRef(c, typeDefIndexOf(pt));
                } catch (...) {
                    extends = 0;
                }
            }
            uint32_t row = c.writer.addTypeDef(metadata.getStringFromIndex(td.namespaceIndex),
                                               metadata.getStringFromIndex(td.nameIndex), td.flags, extends,
                                               td.field_count, td.method_count, td.event_count, td.property_count);
            c.typeRow[i] = row;
        }

        for (int i = c.typeStart; i < c.typeEnd; i++) {
            const auto& td = metadata.typeDefs[static_cast<size_t>(i)];
            for (int j = 0; j < td.nested_type_count; j++) {
                int nested = metadata.nestedTypeIndices[static_cast<size_t>(td.nestedTypesStart + j)];
                auto it = c.typeRow.find(nested);
                if (it != c.typeRow.end()) {
                    c.writer.addNestedClass(it->second, c.typeRow.at(i));
                }
            }
        }

        for (int i = c.typeStart; i < c.typeEnd; i++) {
            const auto& td = metadata.typeDefs[static_cast<size_t>(i)];
            uint32_t typeRow = c.typeRow.at(i);
            if (addToken) {
                addTokenAttribute(c, (typeRow << 5) | Tag::HasCustomAttribute_TypeDef, td.token);
            }
            if (td.genericContainerIndex >= 0) {
                const auto& container = metadata.genericContainers[static_cast<size_t>(td.genericContainerIndex)];
                for (int j = 0; j < container.type_argc; j++) {
                    addGenericParamRow(c, container.genericParameterStart + j, typeRow << 1);
                }
            }
            for (int j = 0; j < td.interfaces_count; j++) {
                int tIdx = metadata.interfaceIndices[static_cast<size_t>(td.interfacesStart + j)];
                try {
                    const auto* it = &il2Cpp.types[static_cast<size_t>(tIdx)];
                    c.writer.addInterfaceImpl(typeRow, codedTypeDefOrRef(c, typeDefIndexOf(it)));
                } catch (...) {
                }
            }
        }

        for (int i : typeOrder) {
            const auto& td = metadata.typeDefs[static_cast<size_t>(i)];
            uint32_t typeRow = c.typeRow.at(i);

            for (int j = 0; j < td.field_count; j++) {
                int fi = td.fieldStart + j;
                const auto& fd = metadata.fieldDefs[static_cast<size_t>(fi)];
                const auto* ft = &il2Cpp.types[static_cast<size_t>(fd.typeIndex)];
                auto tb = typeBytesWithByRef(c, ft);
                uint32_t frow = c.writer.addField(static_cast<uint16_t>(ft->attrs),
                                                  metadata.getStringFromIndex(fd.nameIndex), c.writer.fieldSig(tb));
                c.fieldRow[fi] = frow;
                uint32_t parentCoded = (frow << 5) | Tag::HasCustomAttribute_Field;
                if (addToken) {
                    addTokenAttribute(c, parentCoded, fd.token);
                }
                Il2CppFieldDefaultValue def;
                if (metadata.getFieldDefaultValueFromIndex(fi, def) && def.dataIndex != -1) {
                    BlobValue value;
                    if (executor.tryGetDefaultValue(def.typeIndex, def.dataIndex, value)) {
                        addConstantValue(c, value, (frow << 2) | Tag::HasConstant_Field);
                    } else {
                        addMarkerAttribute(c, "MetadataOffsetAttribute", {{"Offset", "0x"}}, parentCoded);
                    }
                }
                if ((ft->attrs & 0x40) == 0) {
                    int offset = il2Cpp.getFieldOffsetFromIndex(i, j, fi, td.isValueType(), (ft->attrs & 0x10) != 0);
                    if (offset >= 0) {
                        addMarkerAttribute(c, "FieldOffsetAttribute", {{"Offset", hexStr(static_cast<uint64_t>(offset))}},
                                           parentCoded);
                    }
                }
            }

            for (int j = 0; j < td.method_count; j++) {
                int mi = metadata.getMethodIndexFromTypeDefinition(i, j);
                const auto& md = metadata.methodDefs[static_cast<size_t>(mi)];
                const auto* rt = &il2Cpp.types[static_cast<size_t>(md.returnType)];
                auto retBytes = typeBytesWithByRef(c, rt);
                std::vector<std::vector<uint8_t>> paramTypes;
                for (int k = 0; k < md.parameterCount; k++) {
                    const auto& pd = metadata.parameterDefs[static_cast<size_t>(md.parameterStart + k)];
                    paramTypes.push_back(typeBytesWithByRef(c, &il2Cpp.types[static_cast<size_t>(pd.typeIndex)]));
                }
                bool generic = md.genericContainerIndex >= 0;
                uint16_t argc = generic ? static_cast<uint16_t>(
                                              metadata.genericContainers[static_cast<size_t>(md.genericContainerIndex)]
                                                  .type_argc)
                                        : 0;
                bool hasThis = (md.flags & 0x0010) == 0;
                uint32_t sig = c.writer.methodSig(hasThis, generic, argc, retBytes, paramTypes);
                uint32_t mrow = c.writer.addMethodDef(md.iflags, md.flags, metadata.getStringFromIndex(md.nameIndex),
                                                      sig, md.parameterCount);
                c.methodRow[mi] = mrow;
                uint32_t parentCoded = (mrow << 5) | Tag::HasCustomAttribute_MethodDef;
                if (addToken) {
                    addTokenAttribute(c, parentCoded, md.token);
                }
                if (md.genericContainerIndex >= 0) {
                    const auto& container = metadata.genericContainers[static_cast<size_t>(md.genericContainerIndex)];
                    for (int k = 0; k < container.type_argc; k++) {
                        addGenericParamRow(c, container.genericParameterStart + k, (mrow << 1) | 1);
                    }
                }
                for (int k = 0; k < md.parameterCount; k++) {
                    int pi = md.parameterStart + k;
                    const auto& pd = metadata.parameterDefs[static_cast<size_t>(pi)];
                    const auto* pt = &il2Cpp.types[static_cast<size_t>(pd.typeIndex)];
                    uint32_t prow = c.writer.addParam(static_cast<uint16_t>(pt->attrs), static_cast<uint16_t>(k + 1),
                                                      metadata.getStringFromIndex(pd.nameIndex));
                    c.paramRow[pi] = prow;
                    Il2CppParameterDefaultValue pdef;
                    if (metadata.getParameterDefaultValueFromIndex(pi, pdef) && pdef.dataIndex != -1) {
                        BlobValue value;
                        if (executor.tryGetDefaultValue(pdef.typeIndex, pdef.dataIndex, value)) {
                            addConstantValue(c, value, (prow << 2) | Tag::HasConstant_Param);
                        } else {
                            addMarkerAttribute(c, "MetadataOffsetAttribute", {{"Offset", "0x"}},
                                               (prow << 5) | Tag::HasCustomAttribute_Param);
                        }
                    }
                }
                if (hasBody(md) && !isMulticastDelegate(td)) {
                    buildBody(c, rt, mrow);
                }
                if ((md.flags & 0x0400) == 0) {
                    uint64_t methodPointer = il2Cpp.getMethodPointer(c.imageName, md);
                    if (methodPointer > 0) {
                        std::vector<std::pair<std::string, std::string>> fields = {
                            {"RVA", hexStr(il2Cpp.getRVA(methodPointer))},
                            {"Offset", hexStr(il2Cpp.mapVATR(methodPointer))},
                            {"VA", hexStr(methodPointer)},
                        };
                        if (md.slot != 0xFFFF) {
                            fields.emplace_back("Slot", std::to_string(md.slot));
                        }
                        addMarkerAttribute(c, "AddressAttribute", fields, parentCoded);
                    }
                }
            }

            if (td.property_count > 0) {
                c.writer.addPropertyMap(typeRow);
            }
            for (int j = 0; j < td.property_count; j++) {
                int pi = td.propertyStart + j;
                const auto& pd = metadata.propertyDefs[static_cast<size_t>(pi)];
                std::vector<uint8_t> psig = {0x28};
                int idxCount = 0;
                const Il2CppMethodDefinition* accessor = nullptr;
                if (pd.get >= 0) {
                    accessor = &metadata.methodDefs[static_cast<size_t>(metadata.getMethodIndexFromTypeDefinition(i, pd.get))];
                    idxCount = accessor->parameterCount;
                } else if (pd.set >= 0) {
                    accessor = &metadata.methodDefs[static_cast<size_t>(metadata.getMethodIndexFromTypeDefinition(i, pd.set))];
                    idxCount = accessor->parameterCount > 0 ? accessor->parameterCount - 1 : 0;
                }
                const auto* ptype = propertyType(td, pd);
                if (ptype == nullptr) {
                    continue;
                }
                Sig::pushCompressedU(psig, static_cast<uint32_t>(idxCount));
                auto pbytes = typeBytesWithByRef(c, ptype);
                psig.insert(psig.end(), pbytes.begin(), pbytes.end());
                if (idxCount > 0) {
                    for (int k = 0; k < idxCount; k++) {
                        const auto& ipd = metadata.parameterDefs[static_cast<size_t>(accessor->parameterStart + k)];
                        auto ibytes = typeBytesWithByRef(c, &il2Cpp.types[static_cast<size_t>(ipd.typeIndex)]);
                        psig.insert(psig.end(), ibytes.begin(), ibytes.end());
                    }
                }
                uint32_t prow = c.writer.addProperty(static_cast<uint16_t>(pd.attrs),
                                                     metadata.getStringFromIndex(pd.nameIndex),
                                                     c.writer.blobIndex(psig));
                c.propertyRow[pi] = prow;
                uint32_t assoc = (prow << 1) | Tag::HasSemantics_Property;
                if (pd.get >= 0) {
                    c.writer.addMethodSemantics(0x2, c.methodRow.at(metadata.getMethodIndexFromTypeDefinition(i, pd.get)), assoc);
                }
                if (pd.set >= 0) {
                    c.writer.addMethodSemantics(0x1, c.methodRow.at(metadata.getMethodIndexFromTypeDefinition(i, pd.set)), assoc);
                }
                if (addToken) {
                    addTokenAttribute(c, (prow << 5) | Tag::HasCustomAttribute_Property, pd.token);
                }
            }

            if (td.event_count > 0) {
                c.writer.addEventMap(typeRow);
            }
            for (int j = 0; j < td.event_count; j++) {
                int ei = td.eventStart + j;
                const auto& ed = metadata.eventDefs[static_cast<size_t>(ei)];
                const auto* et = &il2Cpp.types[static_cast<size_t>(ed.typeIndex)];
                uint32_t erow = c.writer.addEvent(static_cast<uint16_t>(et->attrs),
                                                  metadata.getStringFromIndex(ed.nameIndex),
                                                  codedTypeDefOrRef(c, typeDefIndexOf(et)));
                c.eventRow[ei] = erow;
                uint32_t assoc = (erow << 1) | Tag::HasSemantics_Event;
                if (ed.add >= 0) {
                    c.writer.addMethodSemantics(0x8, c.methodRow.at(metadata.getMethodIndexFromTypeDefinition(i, ed.add)), assoc);
                }
                if (ed.remove >= 0) {
                    c.writer.addMethodSemantics(0x10, c.methodRow.at(metadata.getMethodIndexFromTypeDefinition(i, ed.remove)), assoc);
                }
                if (ed.raise >= 0) {
                    c.writer.addMethodSemantics(0x20, c.methodRow.at(metadata.getMethodIndexFromTypeDefinition(i, ed.raise)), assoc);
                }
                if (addToken) {
                    addTokenAttribute(c, (erow << 5) | Tag::HasCustomAttribute_Event, ed.token);
                }
            }
        }

        if (il2Cpp.version > 20) {
            for (int i = c.typeStart; i < c.typeEnd; i++) {
                const auto& td = metadata.typeDefs[static_cast<size_t>(i)];
                uint32_t typeRow = c.typeRow.at(i);
                createCustomAttribute(c, imageIndex, td.customAttributeIndex, td.token,
                                      (typeRow << 5) | Tag::HasCustomAttribute_TypeDef);
                for (int j = 0; j < td.field_count; j++) {
                    int fi = td.fieldStart + j;
                    const auto& fd = metadata.fieldDefs[static_cast<size_t>(fi)];
                    createCustomAttribute(c, imageIndex, fd.customAttributeIndex, fd.token,
                                          (c.fieldRow.at(fi) << 5) | Tag::HasCustomAttribute_Field);
                }
                for (int j = 0; j < td.method_count; j++) {
                    int mi = metadata.getMethodIndexFromTypeDefinition(i, j);
                    const auto& md = metadata.methodDefs[static_cast<size_t>(mi)];
                    createCustomAttribute(c, imageIndex, md.customAttributeIndex, md.token,
                                          (c.methodRow.at(mi) << 5) | Tag::HasCustomAttribute_MethodDef);
                    for (int k = 0; k < md.parameterCount; k++) {
                        int pi = md.parameterStart + k;
                        const auto& pd = metadata.parameterDefs[static_cast<size_t>(pi)];
                        createCustomAttribute(c, imageIndex, pd.customAttributeIndex, pd.token,
                                              (c.paramRow.at(pi) << 5) | Tag::HasCustomAttribute_Param);
                    }
                }
                for (int j = 0; j < td.property_count; j++) {
                    int pi = td.propertyStart + j;
                    const auto& pd = metadata.propertyDefs[static_cast<size_t>(pi)];
                    createCustomAttribute(c, imageIndex, pd.customAttributeIndex, pd.token,
                                          (c.propertyRow.at(pi) << 5) | Tag::HasCustomAttribute_Property);
                }
                for (int j = 0; j < td.event_count; j++) {
                    int ei = td.eventStart + j;
                    const auto& ed = metadata.eventDefs[static_cast<size_t>(ei)];
                    createCustomAttribute(c, imageIndex, ed.customAttributeIndex, ed.token,
                                          (c.eventRow.at(ei) << 5) | Tag::HasCustomAttribute_Event);
                }
            }
        }

        assemblies.push_back({c.imageName, c.writer.write()});
    }
};


DummyAssemblyGenerator::DummyAssemblyGenerator(Il2CppExecutor& executor, bool addToken)
    : impl_(std::make_unique<Impl>(executor, addToken)) {
    impl_->buildMarkerAssembly();
    for (size_t i = 0; i < impl_->metadata.imageDefs.size(); i++) {
        try {
            impl_->buildImage(static_cast<int>(i));
        } catch (const std::exception& e) {
            std::cout << "ERROR: Failed to generate dummy dll for image " << i << ": " << e.what() << "\n";
        }
    }
    assemblies = impl_->assemblies;
}

DummyAssemblyGenerator::~DummyAssemblyGenerator() = default;

DummyAssemblyGenerator::DummyAssemblyGenerator(DummyAssemblyGenerator&&) noexcept = default;

}
