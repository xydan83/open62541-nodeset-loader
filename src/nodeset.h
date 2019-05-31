/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2019 (c) Matthias Konnerth
 */

#ifndef NODESET_H
#define NODESET_H
#include "nodesetLoader.h"
#include "util.h"
#include <libxml/SAX.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define MAX_OBJECTTYPES 1000
#define MAX_OBJECTS 100000
#define MAX_METHODS 1000
#define MAX_DATATYPES 1000
#define MAX_VARIABLES 1000000
#define MAX_REFERENCETYPES 1000
#define MAX_HIERACHICAL_REFS 50
#define MAX_ALIAS 100
#define MAX_REFCOUNTEDCHARS 1000000
#define MAX_REFCOUNTEDREFS 1000000

#define OBJECT "UAObject"
#define METHOD "UAMethod"
#define OBJECTTYPE "UAObjectType"
#define VARIABLE "UAVariable"
#define DATATYPE "UADataType"
#define REFERENCETYPE "UAReferenceType"
#define DISPLAYNAME "DisplayName"
#define REFERENCES "References"
#define REFERENCE "Reference"
#define DESCRIPTION "Description"
#define ALIAS "Alias"
#define NAMESPACEURIS "NamespaceUris"
#define NAMESPACEURI "Uri"

typedef struct {
    const char *name;
    const char *defaultValue;
    bool optional;
} NodeAttribute;

extern NodeAttribute attrNodeId;
extern NodeAttribute attrBrowseName;
extern NodeAttribute attrParentNodeId;
extern NodeAttribute attrEventNotifier;
extern NodeAttribute attrDataType;
extern NodeAttribute attrValueRank;
extern NodeAttribute attrArrayDimensions;
extern NodeAttribute attrIsAbstract;
extern NodeAttribute attrIsForward;
extern NodeAttribute attrReferenceType;
extern NodeAttribute attrAlias;

struct Nodeset;
typedef struct Nodeset Nodeset;

extern const char *hierachicalReferences[MAX_HIERACHICAL_REFS];

typedef struct {
    char *name;
    TNodeId id;
} Alias;

typedef struct {
    size_t cnt;
    const TNode **nodes;
} NodeContainer;

struct TParserCtx;
typedef struct TParserCtx TParserCtx;

struct TNamespace;
typedef struct TNamespace TNamespace;

struct TNamespace {
    size_t idx;
    char *name;
};

typedef struct {
    size_t size;
    TNamespace *namespace;
    addNamespaceCb cb;
} TNamespaceTable;

struct Nodeset {
    const Reference **countedRefs;
    const char **countedChars;
    Alias **aliasArray;
    NodeContainer *nodes[NODECLASS_COUNT];
    size_t aliasSize;
    size_t charsSize;
    size_t refsSize;
    TNamespaceTable *namespaceTable;
};

extern Nodeset* nodeset;

void addNodeInternal(const TNode *node);
TNodeId extractNodedId(const TNamespace *namespaces, char *s);
TNodeId translateNodeId(const TNamespace *namespaces, TNodeId id);
TNodeId alias2Id(const char *alias);
void setupNodeset();
void cleanupNodeset();

typedef enum {
    PARSER_STATE_INIT,
    PARSER_STATE_NODE,
    PARSER_STATE_DISPLAYNAME,
    PARSER_STATE_REFERENCES,
    PARSER_STATE_REFERENCE,
    PARSER_STATE_DESCRIPTION,
    PARSER_STATE_ALIAS,
    PARSER_STATE_UNKNOWN,
    PARSER_STATE_NAMESPACEURIS,
    PARSER_STATE_URI
} TParserState;

struct TParserCtx {
    TParserState state;
    TNodeClass nodeClass;
    TNode *node;
    char **nextOnCharacters;
};

#endif
