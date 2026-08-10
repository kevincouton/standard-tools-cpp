#pragma once

#include "standard_tools/audit/storage.hpp"

namespace standard_tools::audit {

class Verifier {
public:
    explicit Verifier(StoragePtr storage);

    void VerifyChain();

private:
    StoragePtr storage_;
};

}  // namespace standard_tools::audit
