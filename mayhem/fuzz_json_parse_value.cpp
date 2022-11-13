#include <stdint.h>
#include <stdio.h>
#include <climits>
#include "json.h"

#include <fuzzer/FuzzedDataProvider.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    std::string str = provider.ConsumeRandomLengthString();
    const char* cstr = str.c_str();

    json_parse_value(cstr);
    return 0;
}