#include "downlist_types.h"
#include "downlist_text_util.h"

struct Replace_Text {
    char *from;
    char *to;
};

struct Replace_Strings {
    char *match_text;
    std::vector<Replace_Text> replace_text;
};

struct File_Contents {
    u32 file_size;
    u8 *contents;
};

static void * alloc(u64 size);
static void free(void *memory);
