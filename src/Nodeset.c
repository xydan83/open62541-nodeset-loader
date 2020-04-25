/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2019 (c) Matthias Konnerth
 */

#include "Nodeset.h"
#include "Sort.h"
#include <AliasList.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool isHierachicalReference(Nodeset *nodeset, const Reference *ref);
TNodeId extractNodedId(const TNamespace *namespaces, char *s);
static TNodeId alias2Id(const Nodeset *nodeset, char *name);

#define MAX_OBJECTTYPES 1000
#define MAX_OBJECTS 100000
#define MAX_METHODS 1000
#define MAX_DATATYPES 1000
#define MAX_VARIABLES 1000000
#define MAX_REFERENCETYPES 1000
#define MAX_VARIABLETYPES 1000
#define MAX_HIERACHICAL_REFS 50

#define MAX_REFCOUNTEDCHARS 10000000
#define MAX_REFCOUNTEDREFS 1000000

// UANode
#define ATTRIBUTE_NODEID "NodeId"
#define ATTRIBUTE_BROWSENAME "BrowseName"
// UAInstance
#define ATTRIBUTE_PARENTNODEID "ParentNodeId"
// UAVariable
#define ATTRIBUTE_DATATYPE "DataType"
#define ATTRIBUTE_VALUERANK "ValueRank"
#define ATTRIBUTE_ARRAYDIMENSIONS "ArrayDimensions"
// UAObject
#define ATTRIBUTE_EVENTNOTIFIER "EventNotifier"
// UAObjectType
#define ATTRIBUTE_ISABSTRACT "IsAbstract"
// Reference
#define ATTRIBUTE_REFERENCETYPE "ReferenceType"
#define ATTRIBUTE_ISFORWARD "IsForward"
#define ATTRIBUTE_SYMMETRIC "Symmetric"
#define ATTRIBUTE_ALIAS "Alias"

typedef struct
{
    const char *name;
    char *defaultValue;
} NodeAttribute;

const NodeAttribute attrNodeId = {ATTRIBUTE_NODEID, NULL};
const NodeAttribute attrBrowseName = {ATTRIBUTE_BROWSENAME, NULL};
const NodeAttribute attrParentNodeId = {ATTRIBUTE_PARENTNODEID, NULL};
const NodeAttribute attrEventNotifier = {ATTRIBUTE_EVENTNOTIFIER, NULL};
const NodeAttribute attrDataType = {ATTRIBUTE_DATATYPE, "i=24"};
const NodeAttribute attrValueRank = {ATTRIBUTE_VALUERANK, "-1"};
const NodeAttribute attrArrayDimensions = {ATTRIBUTE_ARRAYDIMENSIONS, ""};
const NodeAttribute attrIsAbstract = {ATTRIBUTE_ISABSTRACT, "false"};
const NodeAttribute attrIsForward = {ATTRIBUTE_ISFORWARD, "true"};
const NodeAttribute attrReferenceType = {ATTRIBUTE_REFERENCETYPE, NULL};
const NodeAttribute attrAlias = {ATTRIBUTE_ALIAS, NULL};
const NodeAttribute attrExecutable = {"Executable", "true"};
const NodeAttribute attrUserExecutable = {"UserExecutable", "true"};
const NodeAttribute attrAccessLevel = {"AccessLevel", "1"};
const NodeAttribute attrUserAccessLevel = {"UserAccessLevel", "1"};
const NodeAttribute attrSymmetric = {"Symmetric", "false"};

TReferenceTypeNode hierachicalRefs[MAX_HIERACHICAL_REFS] = {
    {NODECLASS_REFERENCETYPE,
     {0, "i=35"},
     {0, "Organizes"},
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL},
    {NODECLASS_REFERENCETYPE,
     {0, "i=36"},
     {0, "HasEventSource"},
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL},
    {NODECLASS_REFERENCETYPE,
     {0, "i=48"},
     {0, "HasNotifier"},
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL},
    {NODECLASS_REFERENCETYPE,
     {0, "i=44"},
     {0, "Aggregates"},
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL},
    {NODECLASS_REFERENCETYPE,
     {0, "i=45"},
     {0, "HasSubtype"},
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL},
    {NODECLASS_REFERENCETYPE,
     {0, "i=47"},
     {0, "HasComponent"},
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL},
    {NODECLASS_REFERENCETYPE,
     {0, "i=46"},
     {0, "HasProperty"},
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL},
    {NODECLASS_REFERENCETYPE,
     {0, "i=47"},
     {0, "HasEncoding"},
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL},
};

TNodeId translateNodeId(const TNamespace *namespaces, TNodeId id)
{
    if (id.nsIdx > 0)
    {
        id.nsIdx = (int)namespaces[id.nsIdx].idx;
        return id;
    }
    return id;
}

TBrowseName translateBrowseName(const TNamespace *namespaces, TBrowseName bn)
{
    if (bn.nsIdx > 0)
    {
        bn.nsIdx = (uint16_t)namespaces[bn.nsIdx].idx;
        return bn;
    }
    return bn;
}

TNodeId extractNodedId(const TNamespace *namespaces, char *s)
{
    if (s == NULL)
    {
        TNodeId id;
        id.id = NULL;
        id.nsIdx = 0;
        return id;
    }
    TNodeId id;
    id.nsIdx = 0;
    char *idxSemi = strchr(s, ';');
    if (idxSemi == NULL)
    {
        id.id = s;
        return id;
    }
    else
    {
        id.nsIdx = atoi(&s[3]);
        id.id = idxSemi + 1;
    }
    return translateNodeId(namespaces, id);
}

TBrowseName extractBrowseName(const TNamespace *namespaces, char *s)
{
    TBrowseName bn;
    bn.nsIdx = 0;
    char *bnName = strchr(s, ':');
    if (bnName == NULL)
    {
        bn.name = s;
        return bn;
    }
    else
    {
        bn.nsIdx = (uint16_t)atoi(&s[0]);
        bn.name = bnName + 1;
    }
    return translateBrowseName(namespaces, bn);
}

static TNodeId alias2Id(const Nodeset *nodeset, char *name)
{
    const TNodeId *alias = AliasList_getNodeId(nodeset->aliasList, name);
    if (!alias)
    {
        return extractNodedId(nodeset->namespaceTable->ns, name);
    }
    return *alias;
}

Nodeset *Nodeset_new(addNamespaceCb nsCallback)
{
    Nodeset *nodeset = (Nodeset *)calloc(1, sizeof(Nodeset));
    nodeset->aliasList = AliasList_new();
    nodeset->countedRefs =
        (Reference **)malloc(sizeof(Reference *) * MAX_REFCOUNTEDREFS);
    nodeset->refsSize = 0;
    nodeset->charArena = CharArenaAllocator_new(1024 * 1024 * 20);
    // objects
    nodeset->nodes[NODECLASS_OBJECT] =
        (NodeContainer *)malloc(sizeof(NodeContainer));
    nodeset->nodes[NODECLASS_OBJECT]->nodes =
        (TNode **)malloc(sizeof(TNode *) * MAX_OBJECTS);
    nodeset->nodes[NODECLASS_OBJECT]->cnt = 0;
    // variables
    nodeset->nodes[NODECLASS_VARIABLE] =
        (NodeContainer *)malloc(sizeof(NodeContainer));
    nodeset->nodes[NODECLASS_VARIABLE]->nodes =
        (TNode **)malloc(sizeof(TNode *) * MAX_VARIABLES);
    nodeset->nodes[NODECLASS_VARIABLE]->cnt = 0;
    // methods
    nodeset->nodes[NODECLASS_METHOD] =
        (NodeContainer *)malloc(sizeof(NodeContainer));
    nodeset->nodes[NODECLASS_METHOD]->nodes =
        (TNode **)malloc(sizeof(TNode *) * MAX_METHODS);
    nodeset->nodes[NODECLASS_METHOD]->cnt = 0;
    // objecttypes
    nodeset->nodes[NODECLASS_OBJECTTYPE] =
        (NodeContainer *)malloc(sizeof(NodeContainer));
    nodeset->nodes[NODECLASS_OBJECTTYPE]->nodes =
        (TNode **)malloc(sizeof(TNode *) * MAX_DATATYPES);
    nodeset->nodes[NODECLASS_OBJECTTYPE]->cnt = 0;
    // datatypes
    nodeset->nodes[NODECLASS_DATATYPE] =
        (NodeContainer *)malloc(sizeof(NodeContainer));
    nodeset->nodes[NODECLASS_DATATYPE]->nodes =
        (TNode **)malloc(sizeof(TNode *) * MAX_DATATYPES);
    nodeset->nodes[NODECLASS_DATATYPE]->cnt = 0;
    // referencetypes
    nodeset->nodes[NODECLASS_REFERENCETYPE] =
        (NodeContainer *)malloc(sizeof(NodeContainer));
    nodeset->nodes[NODECLASS_REFERENCETYPE]->nodes =
        (TNode **)malloc(sizeof(TNode *) * MAX_REFERENCETYPES);
    nodeset->nodes[NODECLASS_REFERENCETYPE]->cnt = 0;
    // variabletypes
    nodeset->nodes[NODECLASS_VARIABLETYPE] =
        (NodeContainer *)malloc(sizeof(NodeContainer));
    nodeset->nodes[NODECLASS_VARIABLETYPE]->nodes =
        (TNode **)malloc(sizeof(TNode *) * MAX_VARIABLETYPES);
    nodeset->nodes[NODECLASS_VARIABLETYPE]->cnt = 0;
    // known hierachical refs
    nodeset->hierachicalRefs = hierachicalRefs;
    nodeset->hierachicalRefsSize = 8;

    TNamespaceTable *table = (TNamespaceTable *)malloc(sizeof(TNamespaceTable));
    table->cb = nsCallback;
    table->size = 1;
    table->ns = (TNamespace *)malloc((sizeof(TNamespace)));
    table->ns[0].idx = 0;
    table->ns[0].name = "http://opcfoundation.org/UA/";
    nodeset->namespaceTable = table;
    Sort_init();
    return nodeset;
}

static void Nodeset_addNode(Nodeset *nodeset, TNode *node)
{
    size_t cnt = nodeset->nodes[node->nodeClass]->cnt;
    nodeset->nodes[node->nodeClass]->nodes[cnt] = node;
    nodeset->nodes[node->nodeClass]->cnt++;
}

bool Nodeset_getSortedNodes(Nodeset *nodeset, void *userContext,
                            addNodeCb callback, ValueInterface *valIf)
{

#ifdef XMLIMPORT_TRACE
    printf("--- namespace table ---\n");
    printf("FileIdx ServerIdx URI\n");
    for (size_t fileIndex = 0; fileIndex < nodeset->namespaceTable->size;
         fileIndex++)
    {
        printf("%zu\t%zu\t%s\n", fileIndex,
               nodeset->namespaceTable->ns[fileIndex].idx,
               nodeset->namespaceTable->ns[fileIndex].name);
    }
#endif

    if (!Sort_start(nodeset, Nodeset_addNode))
    {
        return false;
    }

    for (size_t cnt = 0; cnt < nodeset->nodes[NODECLASS_REFERENCETYPE]->cnt;
         cnt++)
    {
        callback(userContext,
                 nodeset->nodes[NODECLASS_REFERENCETYPE]->nodes[cnt]);
    }

    for (size_t cnt = 0; cnt < nodeset->nodes[NODECLASS_DATATYPE]->cnt; cnt++)
    {
        callback(userContext, nodeset->nodes[NODECLASS_DATATYPE]->nodes[cnt]);
    }

    for (size_t cnt = 0; cnt < nodeset->nodes[NODECLASS_OBJECTTYPE]->cnt; cnt++)
    {
        callback(userContext, nodeset->nodes[NODECLASS_OBJECTTYPE]->nodes[cnt]);
    }

    for (size_t cnt = 0; cnt < nodeset->nodes[NODECLASS_OBJECT]->cnt; cnt++)
    {
        callback(userContext, nodeset->nodes[NODECLASS_OBJECT]->nodes[cnt]);
    }

    for (size_t cnt = 0; cnt < nodeset->nodes[NODECLASS_METHOD]->cnt; cnt++)
    {
        callback(userContext, nodeset->nodes[NODECLASS_METHOD]->nodes[cnt]);
    }

    for (size_t cnt = 0; cnt < nodeset->nodes[NODECLASS_VARIABLETYPE]->cnt;
         cnt++)
    {
        callback(userContext,
                 nodeset->nodes[NODECLASS_VARIABLETYPE]->nodes[cnt]);
    }

    for (size_t cnt = 0; cnt < nodeset->nodes[NODECLASS_VARIABLE]->cnt; cnt++)
    {
        callback(userContext, nodeset->nodes[NODECLASS_VARIABLE]->nodes[cnt]);

        valIf->deleteValue(
            &((TVariableNode *)nodeset->nodes[NODECLASS_VARIABLE]->nodes[cnt])
                 ->value);
    }
    return true;
}

void Nodeset_cleanup(Nodeset *nodeset)
{
    Nodeset *n = nodeset;

    CharArenaAllocator_delete(nodeset->charArena);
    AliasList_delete(nodeset->aliasList);

    // free refs
    for (size_t cnt = 0; cnt < n->refsSize; cnt++)
    {
        free(n->countedRefs[cnt]);
    }
    free(n->countedRefs);

    for (size_t cnt = 0; cnt < NODECLASS_COUNT; cnt++)
    {
        size_t storedNodes = n->nodes[cnt]->cnt;
        for (size_t nodeCnt = 0; nodeCnt < storedNodes; nodeCnt++)
        {
            free(n->nodes[cnt]->nodes[nodeCnt]);
        }
        free((void *)n->nodes[cnt]->nodes);
        free((void *)n->nodes[cnt]);
    }

    free(n->namespaceTable->ns);
    free(n->namespaceTable);
    free(n);
    Sort_cleanup();
}

static bool isHierachicalReference(Nodeset *nodeset, const Reference *ref)
{
    for (size_t i = 0; i < nodeset->hierachicalRefsSize; i++)
    {
        if (!TNodeId_cmp(&ref->refType, &nodeset->hierachicalRefs[i].id))
        {
            return true;
        }
    }
    return false;
}

static char *getAttributeValue(Nodeset *nodeset, const NodeAttribute *attr,
                               const char **attributes, int nb_attributes)
{
    const int fields = 5;
    for (int i = 0; i < nb_attributes; i++)
    {
        const char *localname = attributes[i * fields + 0];
        if (strcmp((const char *)localname, attr->name))
            continue;
        const char *value_start = attributes[i * fields + 3];
        const char *value_end = attributes[i * fields + 4];
        size_t size = (size_t)(value_end - value_start);
        char *value = CharArenaAllocator_malloc(nodeset->charArena, size + 1);
        memcpy(value, value_start, size);
        return value;
    }
    // we return the defaultValue, if NULL or not, following code has to cope
    // with it
    return attr->defaultValue;
}

static void extractAttributes(Nodeset *nodeset, const TNamespace *namespaces,
                              TNode *node, int attributeSize,
                              const char **attributes)
{
    node->id = extractNodedId(
        namespaces,
        getAttributeValue(nodeset, &attrNodeId, attributes, attributeSize));
    node->browseName = extractBrowseName(
        namespaces,
        getAttributeValue(nodeset, &attrBrowseName, attributes, attributeSize));
    switch (node->nodeClass)
    {
    case NODECLASS_OBJECTTYPE:
    {
        ((TObjectTypeNode *)node)->isAbstract = getAttributeValue(
            nodeset, &attrIsAbstract, attributes, attributeSize);
        break;
    }
    case NODECLASS_OBJECT:
    {
        ((TObjectNode *)node)->parentNodeId = extractNodedId(
            namespaces, getAttributeValue(nodeset, &attrParentNodeId,
                                          attributes, attributeSize));
        ((TObjectNode *)node)->eventNotifier = getAttributeValue(
            nodeset, &attrEventNotifier, attributes, attributeSize);
        break;
    }
    case NODECLASS_VARIABLE:
    {

        ((TVariableNode *)node)->parentNodeId = extractNodedId(
            namespaces, getAttributeValue(nodeset, &attrParentNodeId,
                                          attributes, attributeSize));
        char *datatype = getAttributeValue(nodeset, &attrDataType, attributes,
                                           attributeSize);
        ((TVariableNode *)node)->datatype = alias2Id(nodeset, datatype);
        ((TVariableNode *)node)->valueRank = getAttributeValue(
            nodeset, &attrValueRank, attributes, attributeSize);
        ((TVariableNode *)node)->arrayDimensions = getAttributeValue(
            nodeset, &attrArrayDimensions, attributes, attributeSize);
        ((TVariableNode *)node)->accessLevel = getAttributeValue(
            nodeset, &attrAccessLevel, attributes, attributeSize);
        ((TVariableNode *)node)->userAccessLevel = getAttributeValue(
            nodeset, &attrUserAccessLevel, attributes, attributeSize);
        break;
    }
    case NODECLASS_VARIABLETYPE:
    {

        ((TVariableTypeNode *)node)->valueRank = getAttributeValue(
            nodeset, &attrValueRank, attributes, attributeSize);
        char *datatype = getAttributeValue(nodeset, &attrDataType, attributes,
                                           attributeSize);
        ((TVariableTypeNode *)node)->datatype = alias2Id(nodeset, datatype);
        ((TVariableTypeNode *)node)->arrayDimensions = getAttributeValue(
            nodeset, &attrArrayDimensions, attributes, attributeSize);
        ((TVariableTypeNode *)node)->isAbstract = getAttributeValue(
            nodeset, &attrIsAbstract, attributes, attributeSize);
        break;
    }
    case NODECLASS_DATATYPE:;
        break;
    case NODECLASS_METHOD:
        ((TMethodNode *)node)->parentNodeId = extractNodedId(
            namespaces, getAttributeValue(nodeset, &attrParentNodeId,
                                          attributes, attributeSize));
        ((TMethodNode *)node)->executable = getAttributeValue(
            nodeset, &attrExecutable, attributes, attributeSize);
        ((TMethodNode *)node)->userExecutable = getAttributeValue(
            nodeset, &attrUserExecutable, attributes, attributeSize);
        break;
    case NODECLASS_REFERENCETYPE:
        ((TReferenceTypeNode *)node)->symmetric = getAttributeValue(
            nodeset, &attrSymmetric, attributes, attributeSize);
        break;
    default:;
    }
}

static void initNode(Nodeset *nodeset, TNamespace *namespaces,
                     TNodeClass nodeClass, TNode *node, int nb_attributes,
                     const char **attributes)
{
    node->nodeClass = nodeClass;
    node->hierachicalRefs = NULL;
    node->nonHierachicalRefs = NULL;
    node->displayName = NULL;
    node->description = NULL;
    node->writeMask = NULL;
    extractAttributes(nodeset, namespaces, node, nb_attributes, attributes);
}

TNode *Nodeset_newNode(Nodeset *nodeset, TNodeClass nodeClass,
                       int nb_attributes, const char **attributes)
{
    TNode *node = NULL;
    switch (nodeClass)
    {
    case NODECLASS_VARIABLE:
        node = (TNode *)calloc(1, sizeof(TVariableNode));
        break;
    case NODECLASS_OBJECT:
        node = (TNode *)calloc(1, sizeof(TObjectNode));
        break;
    case NODECLASS_OBJECTTYPE:
        node = (TNode *)calloc(1, sizeof(TObjectTypeNode));
        break;
    case NODECLASS_REFERENCETYPE:
        node = (TNode *)calloc(1, sizeof(TReferenceTypeNode));
        break;
    case NODECLASS_VARIABLETYPE:
        node = (TNode *)calloc(1, sizeof(TVariableTypeNode));
        break;
    case NODECLASS_DATATYPE:
        node = (TNode *)calloc(1, sizeof(TDataTypeNode));
        break;
    case NODECLASS_METHOD:
        node = (TNode *)calloc(1, sizeof(TMethodNode));
        break;
    }
    initNode(nodeset, nodeset->namespaceTable->ns, nodeClass, node,
             nb_attributes, attributes);
    return node;
}

static bool isKnownReferenceType(Nodeset *nodeset, const TNodeId *refTypeId)
{
    // we state that we know all references from namespace 0
    if (refTypeId->nsIdx == 0)
    {
        return true;
    }
    for (size_t i = 0; i < nodeset->nodes[NODECLASS_REFERENCETYPE]->cnt; i++)
    {
        if (!TNodeId_cmp(
                refTypeId,
                &nodeset->nodes[NODECLASS_REFERENCETYPE]->nodes[i]->id))
        {
            return true;
        }
    }
    return false;
}

Reference *Nodeset_newReference(Nodeset *nodeset, TNode *node,
                                int attributeSize, const char **attributes)
{
    Reference *newRef = (Reference *)malloc(sizeof(Reference));
    newRef->target.id = NULL;
    newRef->refType.id = NULL;
    nodeset->countedRefs[nodeset->refsSize++] = newRef;
    newRef->next = NULL;
    if (!strcmp("true", getAttributeValue(nodeset, &attrIsForward, attributes,
                                          attributeSize)))
    {
        newRef->isForward = true;
    }
    else
    {
        newRef->isForward = false;
    }
    // TODO: should we check if its an alias
    char *aliasIdString = getAttributeValue(nodeset, &attrReferenceType,
                                            attributes, attributeSize);

    newRef->refType = alias2Id(nodeset, aliasIdString);

    bool isKnownRef = isKnownReferenceType(nodeset, &newRef->refType);
    // TODO: we have to check later on, if it's really a hierachical reference
    // type, otherwise the reference should be marked as non hierachical
    if (isHierachicalReference(nodeset, newRef) || !isKnownRef)
    {
        Reference *lastRef = node->hierachicalRefs;
        node->hierachicalRefs = newRef;
        newRef->next = lastRef;
    }
    else
    {
        Reference *lastRef = node->nonHierachicalRefs;
        node->nonHierachicalRefs = newRef;
        newRef->next = lastRef;
    }
    return newRef;
}

Alias *Nodeset_newAlias(Nodeset *nodeset, int attributeSize,
                        const char **attributes)
{
    return AliasList_newAlias(
        nodeset->aliasList,
        getAttributeValue(nodeset, &attrAlias, attributes, attributeSize));
}

void Nodeset_newAliasFinish(Nodeset *nodeset, Alias *alias, char *idString)
{
    alias->id = extractNodedId(nodeset->namespaceTable->ns, idString);
}

TNamespace *Nodeset_newNamespace(Nodeset *nodeset)
{
    nodeset->namespaceTable->size++;
    TNamespace *ns = (TNamespace *)realloc(nodeset->namespaceTable->ns,
                                           sizeof(TNamespace) *
                                               (nodeset->namespaceTable->size));
    nodeset->namespaceTable->ns = ns;
    ns[nodeset->namespaceTable->size - 1].name = NULL;
    return &ns[nodeset->namespaceTable->size - 1];
}

void Nodeset_newNamespaceFinish(Nodeset *nodeset, void *userContext,
                                char *namespaceUri)
{
    nodeset->namespaceTable->ns[nodeset->namespaceTable->size - 1].name =
        namespaceUri;
    int globalIdx = nodeset->namespaceTable->cb(
        userContext,
        nodeset->namespaceTable->ns[nodeset->namespaceTable->size - 1].name);

    nodeset->namespaceTable->ns[nodeset->namespaceTable->size - 1].idx =
        (size_t)globalIdx;
}

static void addIfHierachicalReferenceType(Nodeset *nodeset, TNode *node)
{
    Reference *ref = node->hierachicalRefs;
    while (ref)
    {
        if (!ref->isForward)
        {
            for (size_t i = 0; i < nodeset->hierachicalRefsSize; i++)
            {
                if (!TNodeId_cmp(&nodeset->hierachicalRefs[i].id, &ref->target))
                {
                    nodeset->hierachicalRefs[nodeset->hierachicalRefsSize++] =
                        *(TReferenceTypeNode *)node;
                    break;
                }
            }
        }
        ref = ref->next;
    }
}

void Nodeset_newNodeFinish(Nodeset *nodeset, TNode *node)
{
    Sort_addNode(node);
    if (node->nodeClass == NODECLASS_REFERENCETYPE)
    {
        addIfHierachicalReferenceType(nodeset, node);
    }
}

void Nodeset_newReferenceFinish(Nodeset *nodeset, Reference *ref, TNode *node,
                                char *targetId)
{
    ref->target = extractNodedId(nodeset->namespaceTable->ns, targetId);
}