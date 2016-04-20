#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "index.h"
#include "varint.h"
#include "redismodule.h"
#include "forward_index.h"
#include "tokenize.h"
#include "redis_index.h"

typedef struct {
    const char *name;
    const char *text;
} DocumentField;

typedef struct {
    RedisModuleString *docKey;
    DocumentField *fields;
    int numFiels;
    float score; 
} Document;

typedef struct {
    t_docId *docIds;
    size_t totalResults;
    t_offset limit;
} Result;

int AddDocument(RedisModuleCtx *ctx, Document doc) {
    
    int isnew;
    t_docId docId = Redis_GetDocId(ctx, doc.docKey, &isnew);
    if (docId == 0 || isnew == 0) {
        LG_ERROR("Not a new doc");
        return REDISMODULE_ERR;
    }
    
    ForwardIndex *idx = NewForwardIndex(1,  doc.score);
    
    
    int totalTokens = 0;
    for (int i = 0; i < doc.numFiels; i++) {
        totalTokens += tokenize(doc.fields[i].text, 1, 1, idx, forwardIndexTokenFunc);        
    }
    
    if (totalTokens > 0) {
        ForwardIndexIterator it = ForwardIndex_Iterate(idx);
        
        ForwardIndexEntry *entry = ForwardIndexIterator_Next(&it);
        while (entry != NULL) {
            
            IndexWriter *w = Redis_OpenWriter(ctx, entry->term);
            IW_WriteEntry(w, entry);
            Redis_CloseWriter(w);
        }
        
    }
    ForwardIndexFree(idx);
    return 0;
}


Result *Search(const char *query) {
    return NULL;
}

/*
FT.ADD <docId> <score> [<field> <text>, ....] 
*/
int AddDocumentCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    
    // at least one field, and number of field/text args must be even
    if (argc < 5 || (argc - 3) % 2 == 1) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);
    
    Document doc;
    doc.docKey = argv[1];
    doc.score = 1.0;
    doc.fields = calloc((argc-3)/2, sizeof(DocumentField));
    for (int i = 3; i < argc; i+=2) {
        doc.fields[i].name = RedisModule_StringPtrLen(argv[i], NULL);
        doc.fields[i].text = RedisModule_StringPtrLen(argv[i+1], NULL);
    }
    
    int rc = AddDocument(ctx, doc);
    if (rc == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "Could not index document");
    } else {
        RedisModule_ReplyWithSimpleString(ctx, "OK");
    }
    
    return REDISMODULE_OK;
    
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {
    
    
    if (RedisModule_Init(ctx,"ft",1,REDISMODULE_APIVER_1)
        == REDISMODULE_ERR) return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx,"ft.add",
        AddDocumentCommand) == REDISMODULE_ERR)
        return REDISMODULE_ERR;
        
//  if (RedisModule_CreateCommand(ctx,"hgetset",
//         HGetSetCommand) == REDISMODULE_ERR)
//         return REDISMODULE_ERR;

    return REDISMODULE_OK;
}