#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "dynamiclinker.h"
#include "external.h"
#include "metadata.h"
#define XOVI_ROOT_DEFAULT "/home/root/xovi"
#define EXT_ROOT extRootDir
#define FILES_ROOT filesRootDir

static const char *extRootDir = NULL, *filesRootDir = NULL;
static int xoviInitialized = 0;

void _ext_init(void);

static const char *pathBaseName(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static int isCurrentXoviObject(const char *candidate, const Dl_info *self) {
    if(self != NULL && self->dli_fname != NULL) {
        struct stat candidateStat, selfStat;
        if(stat(candidate, &candidateStat) == 0 && stat(self->dli_fname, &selfStat) == 0 &&
           candidateStat.st_dev == selfStat.st_dev && candidateStat.st_ino == selfStat.st_ino) {
            return 1;
        }
        if(strcmp(candidate, self->dli_fname) == 0) return 1;
        if(strchr(candidate, '/') == NULL &&
           strcmp(candidate, pathBaseName(self->dli_fname)) == 0) {
            return 1;
        }
        return 0;
    }

    return strcmp(pathBaseName(candidate), "xovi.so") == 0;
}

static void configureChildInjection(void) {
    const char *setting = getenv("XOVI_INJECT_CHILDREN");
    const char *preload = getenv("LD_PRELOAD");
    if((setting != NULL && strcmp(setting, "1") == 0) ||
       preload == NULL || *preload == '\0') return;

    Dl_info self = {0};
    Dl_info *selfPtr = dladdr((void *)&_ext_init, &self) != 0 ? &self : NULL;
    char *copy = strdup(preload);
    char *sanitized = calloc(strlen(preload) + 1, 1);
    if(copy == NULL || sanitized == NULL) {
        free(copy);
        free(sanitized);
        return;
    }

    size_t length = 0;
    char *save = NULL;
    for(char *entry = strtok_r(copy, " :\t\r\n", &save);
        entry != NULL;
        entry = strtok_r(NULL, " :\t\r\n", &save)) {
        if(isCurrentXoviObject(entry, selfPtr)) continue;
        if(length != 0) sanitized[length++] = ':';
        size_t entryLength = strlen(entry);
        memcpy(sanitized + length, entry, entryLength);
        length += entryLength;
    }

    if(length == 0) unsetenv("LD_PRELOAD");
    else setenv("LD_PRELOAD", sanitized, 1);
    free(sanitized);
    free(copy);
}

#ifdef DEBUGFUNC
#define CONSTRUCTOR

int testFunc() {
    printf("In playground\n");
}

int main(){
    _ext_init();
}
#else
#define CONSTRUCTOR __attribute__((constructor))
#endif

char *findBaseName(const char *fileName) {
    const char *extStart = strchr(fileName, '.');
    if(extStart == NULL) return strdup(fileName); // No '.'? The whole name is baseName
    char *allocd = malloc(extStart - fileName + 1);
    allocd[extStart - fileName] = 0; // NULL-term
    memcpy(allocd, fileName, extStart - fileName);
    return allocd;
}

char *mstrcat(const char *a, const char *b, int additionalSpace, char **end) {
    int aLength = strlen(a), bLength = strlen(b);
    char *c = calloc(1, aLength + bLength + 1 + additionalSpace);
    memcpy(c, a, aLength);
    memcpy(c + aLength, b, bLength);
    c[aLength + bLength] = 0;
    if(end) {
        *end = &c[aLength + bLength];
    }
    return c;
}

char *findFullName(const char *fileName) {
    return mstrcat(EXT_ROOT, fileName, 0, NULL);
}

char *getExtensionDirectory(const char *family){
    char *slash, *ret = mstrcat(FILES_ROOT, family, 1, &slash);
    slash[0] = '/';
    slash[1] = 0;
    return ret;
}

static char *concat(const char *a, const char *b) {
    int alen = strlen(a), blen = strlen(b);
    char *newString = malloc(alen + blen + 1);
    memcpy(newString, a, alen);
    memcpy(newString + alen, b, blen);
    newString[alen + blen] = 0;
    return newString;
}

void CONSTRUCTOR _ext_init() {
    if(xoviInitialized) {
        LOG("[I]: XOVI initialization skipped - already initialized in this process.\n");
        return;
    }
    xoviInitialized = 1;
    configureChildInjection();
    
    const char *xoviRoot;
    if((xoviRoot = getenv("XOVI_ROOT")) == NULL) xoviRoot = XOVI_ROOT_DEFAULT;
    extRootDir = concat(xoviRoot, "/extensions.d/");
    filesRootDir = concat(xoviRoot, "/exthome/");

    struct XoViEnvironment *environment = malloc(sizeof(struct XoViEnvironment));
    environment->getExtensionDirectory = getExtensionDirectory;

    // 0.2.0 API:
    environment->getExtensionCount = getExtensionCount;
    environment->getExtensionNames = getExtensionNames;
    environment->getExtensionFunctionCount = getExtensionFunctionCount;
    environment->getExtensionFunctionNames = getExtensionFunctionNames;

    environment->getMetadataEntriesCountForFunction = getMetadataEntriesCountForFunction;
    environment->getMetadataChainForFunction = getMetadataChainForFunction;
    environment->getMetadataEntryForFunction = getMetadataEntryForFunction;

    // Cast required due to differences between the public and private Iterator structure
    environment->createMetadataSearchingIterator = (void (*)(struct ExtensionMetadataIterator *, const char *)) createMetadataSearchingIterator;
    environment->nextFunctionMetadataEntry = (struct XoviMetadataEntry *(*)(struct ExtensionMetadataIterator *)) nextFunctionMetadataEntry;

    environment->getScannedExtensionCount = getScannedExtensionCount;
    environment->getScannedExtensionNames = getScannedExtensionNames;
    environment->getExtensionVersion = getExtensionVersion;
    environment->getExtensionMetadataEntry = getExtensionMetadataEntry;
    environment->getExtensionLoadState = getExtensionLoadState;
    environment->getExtensionLoadError = getExtensionLoadError;

    // At this point none of the functions could have been hooked.
    // It's safe to use stdlib.
    DIR *rootDirOfExtensions;
    struct dirent *entry;

    if((rootDirOfExtensions = opendir(EXT_ROOT)) != NULL){
        // Try to load every extension
        while((entry = readdir(rootDirOfExtensions)) != NULL) {
            if(entry->d_type == DT_REG || entry->d_type == DT_LNK){
                // LOG("Loading: %s\n", entry->d_name);
                char *baseName = findBaseName(entry->d_name);
                char *fullName = findFullName(entry->d_name);
                loadExtensionPass1(fullName, baseName);
                free(fullName);
            }
        }
        closedir(rootDirOfExtensions);
        loadAllExtensions(environment);

        #ifdef DEBUGFUNC
        // Only when we're in debug (non-shared) do we actually clean up in the env. Otherwise this object should stay resident for the whole life of the
        // attached process.
        testFunc();
        free(environment);
        #endif
    } else {
        printf("Cannot find extensions dir! Bailing!\n");
    }
}
