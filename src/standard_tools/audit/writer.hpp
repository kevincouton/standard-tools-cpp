#pragma once

#include "standard_tools/audit/record.hpp"
#include "standard_tools/audit/storage.hpp"

#include <mutex>

namespace standard_tools::audit {

class Writer {
public:
    explicit Writer(StoragePtr storage);

    void Write(DecisionRecord record);

private:
    StoragePtr storage_;
    std::mutex mu_;
};

}  // namespace standard_tools::audit
