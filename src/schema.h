#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

namespace schema {

constexpr uintptr_t SCHEMA_SYSTEM_TYPE_SCOPES_OFFSET = 0x190;
constexpr uintptr_t TYPE_SCOPE_NAME_OFFSET           = 0x8;
constexpr uintptr_t TYPE_SCOPE_HASH_TABLE_OFFSET     = 0x558;

constexpr uintptr_t HASH_TABLE_DATA_OFFSET           = 0x0;
constexpr uintptr_t HASH_TABLE_ALLOC_COUNT_OFFSET    = 0x4;
constexpr uintptr_t HASH_BUCKET_SIZE                 = 0x18;
constexpr uintptr_t HASH_BUCKET_DATA_OFFSET          = 0x10;
constexpr uintptr_t HASH_UNALLOC_DATA_OFFSET         = 0x18;
constexpr uintptr_t HASH_UNALLOC_NEXT_OFFSET         = 0x0;

constexpr uintptr_t CLASS_BINDING_NAME_OFFSET        = 0x8;
constexpr uintptr_t CLASS_BINDING_FIELDS_COUNT_OFFSET = 0x1C;
constexpr uintptr_t CLASS_BINDING_FIELDS_OFFSET      = 0x28;

constexpr uintptr_t FIELD_DATA_SIZE                  = 0x20;
constexpr uintptr_t FIELD_NAME_OFFSET                = 0x0;
constexpr uintptr_t FIELD_SINGLE_OFFSET              = 0x10;

int32_t FindFieldOffset(HANDLE process, DWORD pid,
                        const std::string& scopeName,
                        const std::string& className,
                        const std::string& fieldName);

} // namespace schema
