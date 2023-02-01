#pragma once

// A JSON object consists of text enclosed within matching
// curly braces.
typedef struct JsonObject {
    char *start;    // points to the left curly brace where the object starts
    char *end;      // points to the right curly brace where the object ends
} JsonObject;


#ifdef __cplusplus
extern "C" {
#endif

// Search the specified buffer for the outermost JSON object: e.g.
//
//    {"user':"John Doe","age":"35","gender":"male","hobbies"={...}}
//
extern int jsonFindObject(const char *data, size_t dataLen, JsonObject *pObj);

// Dump the JSON object
extern void jsonDumpObject(const JsonObject *pObj);

// Locate the specified tag within the given JSON object and
// return a pointer to its value: e.g.
//
//   { ..., <tag> : <value>, ... }
//
extern const char *jsonFindTag(const JsonObject *pObj, const char *tag);

// Format is: "<tag>":"<val>" where the value is a string in
// double quotes.
extern char *jsonGetTagValue(const JsonObject *pObj, const char *tag);

#ifdef __cplusplus
}
#endif


