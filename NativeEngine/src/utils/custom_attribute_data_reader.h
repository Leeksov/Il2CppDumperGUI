#pragma once

#include "io/binary_stream.h"
#include "utils/blob_value.h"
#include "utils/il2cpp_executor.h"

#include <cstdint>
#include <vector>

namespace il2cppdumper {

struct AttributeArgument {
    int index = 0;
    BlobValue value;
};

struct AttributeVisitor {
    int ctorIndex = -1;
    std::vector<AttributeArgument> arguments;
    std::vector<AttributeArgument> fields;
    std::vector<AttributeArgument> properties;
};

class CustomAttributeDataReader : public BinaryStream {
public:
    CustomAttributeDataReader(Il2CppExecutor& executor, std::vector<uint8_t> buff);

    uint32_t count() const { return count_; }

    std::string getStringCustomAttributeData();
    AttributeVisitor visitCustomAttributeData();

private:
    std::string attributeDataToString(const BlobValue& blobValue);
    BlobValue readAttributeDataValue();
    std::pair<int, int> readCustomAttributeNamedArgumentClassAndIndex(int currentTypeDefIndex);

    Il2CppExecutor& executor;
    Metadata* metadata;
    size_t ctorBuffer = 0;
    size_t dataBuffer = 0;
    uint32_t count_ = 0;
};

}
