#ifndef XOVI_PUBLIC_API
#define XOVI_PUBLIC_API
#define XOVI_VERSION "0.3.0"
#include <stdbool.h>

#define LP1_F_TYPE_EXPORT 1
#define LP1_F_TYPE_IMPORT 2
#define LP1_F_TYPE_OVERRIDE 3
#define LP1_F_TYPE_CONDITION 4

#define METADATA_TYPE_INT 1
#define METADATA_TYPE_BOOL 2
#define METADATA_TYPE_STRING 3

#define XOVI_EXTENSION_DISCOVERED 1
#define XOVI_EXTENSION_DLOPEN_FAILED 2
#define XOVI_EXTENSION_SHOULDLOAD_FAILED 3
#define XOVI_EXTENSION_CONDITION_FAILED 4
#define XOVI_EXTENSION_DEPENDENCY_FAILED 5
#define XOVI_EXTENSION_LINK_FAILED 6
#define XOVI_EXTENSION_INITIALIZED 7

typedef union {
    int i;
    bool b;
    struct {
        int sLength;
        const char *s;
    };
} XoviMetadataValue;

struct XoviMetadataEntry {
    const char *name;
    char type;
    XoviMetadataValue value;
};

// Public version of the metadata iterator.
struct ExtensionMetadataIterator {
    const char *extensionName;
    const char *functionName;
    void *functionAddress;
    char OPAQUE[sizeof(void *) * 4 + sizeof(bool)];
};

struct XoViEnvironment {
    char *(*getExtensionDirectory)(const char *family);
    void (*requireExtension)(const char *name, unsigned char major, unsigned char minor, unsigned char patch);

    // 0.2.0 API - metadata:
    int (*getExtensionCount)();
    int (*getExtensionNames)(const char **table, int maxCount);
    int (*getExtensionFunctionCount)(const char *key);
    int (*getExtensionFunctionNames)(const char *extension, const char **table, int maxCount);

    int (*getMetadataEntriesCountForFunction)(const char *extension, const char *function, int functionType);
    struct XoviMetadataEntry **(*getMetadataChainForFunction)(const char *extension, const char *function, int functionType);
    struct XoviMetadataEntry *(*getMetadataEntryForFunction)(const char *extension, const char *function, int functionType, const char *metadataEntryName);

    void (*createMetadataSearchingIterator)(struct ExtensionMetadataIterator *iterator, const char *metadataEntryName);
    struct XoviMetadataEntry *(*nextFunctionMetadataEntry)(struct ExtensionMetadataIterator *iterator);

    // 0.3.0 API - runtime extension state:
    int (*getScannedExtensionCount)();
    int (*getScannedExtensionNames)(const char **table, int maxCount);
    int (*getExtensionVersion)(const char *extension, unsigned char *major, unsigned char *minor, unsigned char *patch);
    struct XoviMetadataEntry *(*getExtensionMetadataEntry)(const char *extension, const char *metadataEntryName);
    int (*getExtensionLoadState)(const char *extension);
    const char *(*getExtensionLoadError)(const char *extension);
};
#endif
