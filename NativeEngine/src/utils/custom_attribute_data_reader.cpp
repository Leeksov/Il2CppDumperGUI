#include "utils/custom_attribute_data_reader.h"

#include "utils/string_utils.h"

#include <string>
#include <utility>

namespace il2cppdumper {

CustomAttributeDataReader::CustomAttributeDataReader(Il2CppExecutor& executor, std::vector<uint8_t> buff)
    : BinaryStream(std::move(buff)), executor(executor), metadata(executor.metadata) {
    count_ = readCompressedU32();
    ctorBuffer = position();
    dataBuffer = position() + static_cast<size_t>(count_) * 4;
}

std::string CustomAttributeDataReader::getStringCustomAttributeData() {
    setPosition(ctorBuffer);
    int32_t ctorIndex = readI32();
    const auto& methodDef = metadata->methodDefs[static_cast<size_t>(ctorIndex)];
    const auto& typeDef = metadata->typeDefs[static_cast<size_t>(methodDef.declaringType)];
    ctorBuffer = position();

    setPosition(dataBuffer);
    uint32_t argumentCount = readCompressedU32();
    uint32_t fieldCount = readCompressedU32();
    uint32_t propertyCount = readCompressedU32();

    std::vector<std::string> argList;

    for (uint32_t i = 0; i < argumentCount; i++) {
        argList.push_back(attributeDataToString(readAttributeDataValue()));
    }
    for (uint32_t i = 0; i < fieldCount; i++) {
        std::string str = attributeDataToString(readAttributeDataValue());
        auto named = readCustomAttributeNamedArgumentClassAndIndex(static_cast<int>(&typeDef - metadata->typeDefs.data()));
        const auto& declaringDef = metadata->typeDefs[static_cast<size_t>(named.first)];
        const auto& fDef = metadata->fieldDefs[static_cast<size_t>(declaringDef.fieldStart + named.second)];
        argList.push_back(metadata->getStringFromIndex(fDef.nameIndex) + " = " + str);
    }
    for (uint32_t i = 0; i < propertyCount; i++) {
        std::string str = attributeDataToString(readAttributeDataValue());
        auto named = readCustomAttributeNamedArgumentClassAndIndex(static_cast<int>(&typeDef - metadata->typeDefs.data()));
        const auto& declaringDef = metadata->typeDefs[static_cast<size_t>(named.first)];
        const auto& pDef = metadata->propertyDefs[static_cast<size_t>(declaringDef.propertyStart + named.second)];
        argList.push_back(metadata->getStringFromIndex(pDef.nameIndex) + " = " + str);
    }
    dataBuffer = position();

    std::string typeName = metadata->getStringFromIndex(typeDef.nameIndex);
    size_t pos = 0;
    while ((pos = typeName.find("Attribute", pos)) != std::string::npos) {
        typeName.erase(pos, 9);
    }
    if (!argList.empty()) {
        std::string joined;
        for (size_t i = 0; i < argList.size(); i++) {
            if (i != 0) {
                joined += ", ";
            }
            joined += argList[i];
        }
        return "[" + typeName + "(" + joined + ")]";
    }
    return "[" + typeName + "]";
}

AttributeVisitor CustomAttributeDataReader::visitCustomAttributeData() {
    AttributeVisitor visitor;

    setPosition(ctorBuffer);
    int32_t ctorIndex = readI32();
    visitor.ctorIndex = ctorIndex;
    const auto& methodDef = metadata->methodDefs[static_cast<size_t>(ctorIndex)];
    const auto& typeDef = metadata->typeDefs[static_cast<size_t>(methodDef.declaringType)];
    ctorBuffer = position();

    setPosition(dataBuffer);
    uint32_t argumentCount = readCompressedU32();
    uint32_t fieldCount = readCompressedU32();
    uint32_t propertyCount = readCompressedU32();

    int currentTypeDefIndex = static_cast<int>(&typeDef - metadata->typeDefs.data());

    visitor.arguments.resize(argumentCount);
    for (uint32_t i = 0; i < argumentCount; i++) {
        visitor.arguments[i].index = static_cast<int>(i);
        visitor.arguments[i].value = readAttributeDataValue();
    }
    visitor.fields.resize(fieldCount);
    for (uint32_t i = 0; i < fieldCount; i++) {
        visitor.fields[i].value = readAttributeDataValue();
        auto named = readCustomAttributeNamedArgumentClassAndIndex(currentTypeDefIndex);
        visitor.fields[i].index = metadata->typeDefs[static_cast<size_t>(named.first)].fieldStart + named.second;
    }
    visitor.properties.resize(propertyCount);
    for (uint32_t i = 0; i < propertyCount; i++) {
        visitor.properties[i].value = readAttributeDataValue();
        auto named = readCustomAttributeNamedArgumentClassAndIndex(currentTypeDefIndex);
        visitor.properties[i].index =
            metadata->typeDefs[static_cast<size_t>(named.first)].propertyStart + named.second;
    }

    dataBuffer = position();
    return visitor;
}

std::string CustomAttributeDataReader::attributeDataToString(const BlobValue& blobValue) {
    if (blobValue.kind == BlobValue::Kind::None) {
        return "null";
    }
    switch (blobValue.il2CppTypeEnum) {
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
            return "\"" + blobValue.str + "\"";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
            std::vector<std::string> list;
            for (const auto& item : blobValue.array) {
                list.push_back(attributeDataToString(item));
            }
            std::string joined;
            for (size_t i = 0; i < list.size(); i++) {
                if (i != 0) {
                    joined += ", ";
                }
                joined += list[i];
            }
            return "new[] { " + joined + " }";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_IL2CPP_TYPE_INDEX): {
            if (blobValue.typeIndex >= executor.il2Cpp->types.size()) {
                return "typeof(object)";
            }
            const Il2CppType& type = executor.il2Cpp->types[blobValue.typeIndex];
            return "typeof(" + executor.getTypeName(&type, false, false) + ")";
        }
        default:
            switch (blobValue.kind) {
                case BlobValue::Kind::Bool:
                    return blobValue.b ? "True" : "False";
                case BlobValue::Kind::I8:
                    return std::to_string(blobValue.i8);
                case BlobValue::Kind::U8:
                    return std::to_string(blobValue.u8);
                case BlobValue::Kind::I16:
                    return std::to_string(blobValue.i16);
                case BlobValue::Kind::U16:
                    return std::to_string(blobValue.u16);
                case BlobValue::Kind::I32:
                    return std::to_string(blobValue.i32);
                case BlobValue::Kind::U32:
                    return std::to_string(blobValue.u32);
                case BlobValue::Kind::I64:
                    return std::to_string(blobValue.i64);
                case BlobValue::Kind::U64:
                    return std::to_string(blobValue.u64);
                case BlobValue::Kind::F32:
                    return netNumberFormat(blobValue.f32, 9);
                case BlobValue::Kind::F64:
                    return netNumberFormat(blobValue.f64, 17);
                default:
                    return "";
            }
    }
}

BlobValue CustomAttributeDataReader::readAttributeDataValue() {
    uint32_t enumTypeIndex = 0;
    int type = executor.readEncodedTypeEnum(*this, enumTypeIndex);
    BlobValue blobValue;
    executor.getConstantValueFromBlob(*this, type, blobValue);
    if (enumTypeIndex != 0) {
        blobValue.enumTypeIndex = enumTypeIndex;
    }
    return blobValue;
}

std::pair<int, int> CustomAttributeDataReader::readCustomAttributeNamedArgumentClassAndIndex(int currentTypeDefIndex) {
    int32_t memberIndex = readCompressedI32();
    if (memberIndex >= 0) {
        return {currentTypeDefIndex, memberIndex};
    }
    memberIndex = -(memberIndex + 1);
    uint32_t typeIndex = readCompressedU32();
    return {static_cast<int>(typeIndex), memberIndex};
}

}
