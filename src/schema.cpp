#include "schema.h"
#include "memory.h"
#include "scanner.h"
#include <vector>

namespace schema {

static uintptr_t FindSchemaSystemSingleton(HANDLE process, DWORD pid) {
    auto modInfo = mem::GetModuleInfo(pid, L"schemasystem.dll");
    if (!modInfo.base) return 0;

    PatternScanner scanner(process, modInfo.base, modInfo.size);

    std::vector<PatternEntry> patterns = {
        { "48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8D 4C 24 40", 3, 7 },
        { "48 8B 0D ? ? ? ? 48 85 C9 75 ? 48 8D 0D", 3, 7 },
        { "48 89 05 ? ? ? ? 4C 8D 45", 3, 7 },
        { "48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8B D0 4C 8D 45", 3, 7 },
    };

    return scanner.FindFirstRelative(patterns);
}

static uintptr_t FindTypeScope(HANDLE process, uintptr_t schemaSystem, const std::string& scopeName) {
    uintptr_t scopesPtr = mem::RPM<uintptr_t>(process, schemaSystem + SCHEMA_SYSTEM_TYPE_SCOPES_OFFSET);
    int32_t scopeCount = mem::RPM<int32_t>(process, schemaSystem + SCHEMA_SYSTEM_TYPE_SCOPES_OFFSET + 0x8);

    if (!scopesPtr || scopeCount <= 0 || scopeCount > 64)
        return 0;

    for (int i = 0; i < scopeCount; i++) {
        uintptr_t scope = mem::RPM<uintptr_t>(process, scopesPtr + i * 8);
        if (!scope) continue;

        std::string name = mem::ReadString(process, scope + TYPE_SCOPE_NAME_OFFSET, 64);
        if (name == scopeName)
            return scope;
    }
    return 0;
}

static uintptr_t FindClassBinding(HANDLE process, uintptr_t typeScope, const std::string& className) {
    uintptr_t hashBase = typeScope + TYPE_SCOPE_HASH_TABLE_OFFSET;

    int32_t allocCount = mem::RPM<int32_t>(process, hashBase + HASH_TABLE_ALLOC_COUNT_OFFSET);
    if (allocCount <= 0 || allocCount > 65536)
        return 0;

    uintptr_t bucketsPtr = mem::RPM<uintptr_t>(process, hashBase + HASH_TABLE_DATA_OFFSET);
    uintptr_t unallocBase = hashBase + HASH_UNALLOC_DATA_OFFSET;

    for (int i = 0; i < allocCount; i++) {
        uintptr_t bucket = bucketsPtr + i * HASH_BUCKET_SIZE;
        uintptr_t bindingPtr = mem::RPM<uintptr_t>(process, bucket + HASH_BUCKET_DATA_OFFSET);

        while (bindingPtr) {
            uintptr_t namePtr = mem::RPM<uintptr_t>(process, bindingPtr + CLASS_BINDING_NAME_OFFSET);
            if (namePtr) {
                std::string name = mem::ReadString(process, namePtr, 128);
                if (name == className)
                    return bindingPtr;
            }

            uintptr_t nextIndex = mem::RPM<uintptr_t>(process, bindingPtr);
            if (nextIndex == 0 || nextIndex == UINTPTR_MAX)
                break;
            bindingPtr = unallocBase + nextIndex * HASH_BUCKET_SIZE + HASH_BUCKET_DATA_OFFSET;
            bindingPtr = mem::RPM<uintptr_t>(process, bindingPtr);
        }
    }
    return 0;
}

int32_t FindFieldOffset(HANDLE process, DWORD pid,
                        const std::string& scopeName,
                        const std::string& className,
                        const std::string& fieldName)
{
    uintptr_t schemaSystem = FindSchemaSystemSingleton(process, pid);
    if (!schemaSystem) return -1;

    uintptr_t typeScope = FindTypeScope(process, schemaSystem, scopeName);
    if (!typeScope) return -1;

    uintptr_t classBinding = FindClassBinding(process, typeScope, className);
    if (!classBinding) return -1;

    int16_t fieldCount = mem::RPM<int16_t>(process, classBinding + CLASS_BINDING_FIELDS_COUNT_OFFSET);
    uintptr_t fieldsPtr = mem::RPM<uintptr_t>(process, classBinding + CLASS_BINDING_FIELDS_OFFSET);

    if (!fieldsPtr || fieldCount <= 0 || fieldCount > 4096)
        return -1;

    for (int i = 0; i < fieldCount; i++) {
        uintptr_t field = fieldsPtr + i * FIELD_DATA_SIZE;
        uintptr_t namePtr = mem::RPM<uintptr_t>(process, field + FIELD_NAME_OFFSET);
        if (!namePtr) continue;

        std::string name = mem::ReadString(process, namePtr, 64);
        if (name == fieldName) {
            return mem::RPM<int32_t>(process, field + FIELD_SINGLE_OFFSET);
        }
    }
    return -1;
}

} // namespace schema
