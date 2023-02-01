#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <unistd.h>

#include "json.h"

// Create a null-terminated string with the characters
// between 'start' and 'end' inclusive.
static char *stringify(const char *start, const char *end)
{
    char *str;
    size_t len = end - start + 1;

    if ((str = malloc(len+1)) != NULL) {
        memcpy(str, start, len);
        str[len] = '\0';
    }

    return str;
}

// Search the specified buffer for the outermost JSON object: e.g.
//
//    {"user':"John Doe","age":"35","gender":"male"}
//
int jsonFindObject(const char *data, size_t dataLen, JsonObject *pObj)
{
    // Locate the left curly brace
    if ((pObj->start = strchr(data, '{')) != NULL) {
        int level = 0;
        const char *pEnd = data + dataLen;

        // Locate the matching right curly brace which
        // terminates the JSON object.
        for (char *p = pObj->start; p != pEnd; p++) {
            if (*p == '{') {
                level++;
            } else if (*p == '}') {
                level--;
            }
            if (level == 0) {
                pObj->end = p;
                return 0;
            }
        }
    }

    return -1;
}

static void dumpText(const char *data, size_t dataLen)
{
    const char *pEnd = data + dataLen;
    for (const char *p = data; p <= pEnd; p++) {
        fputc(*p, stdout);
    }
    fputc('\n', stdout);
    fflush(stdout);
}

// Dump the JSON object
void jsonDumpObject(const JsonObject *pObj)
{
    dumpText(pObj->start, (pObj->end - pObj->start));
}

// Locate the specified tag within the given JSON object and
// return a pointer to its value: e.g.
//
//   { ..., <tag> : <value>, ... }
//
const char *jsonFindTag(const JsonObject *pObj, const char *tag)
{
    char label[256];
    size_t len;

    snprintf(label, sizeof (label), "\"%s\"", tag);
    len = strlen(label);
    for (const char *p = (pObj->start + 1); p < pObj->end; p++) {
        if (memcmp(p, label, len) == 0) {
            for (p += len; p < pObj->end; p++) {
                int c = *p;
                if (isspace(c) || (c == ':'))
                    continue;
                return p;
            }
        }
    }

    return NULL;
}

// Format is: "<tag>":"<val>" where the value is a string in
// double quotes.
char *jsonGetTagValue(const JsonObject *pObj, const char *tag)
{
    const char *val;

    if ((val = jsonFindTag(pObj, tag)) != NULL) {
        const char *openQuotes = strchr(val, '"');
        if (openQuotes != NULL) {
            const char *endQuotes = strchr((openQuotes+1), '"');
            if (endQuotes != NULL) {
                return stringify((openQuotes+1), (endQuotes - 1));
            }
        }
    }

    return NULL;
}



