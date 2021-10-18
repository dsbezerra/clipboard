static int
string_length(char *str) {
	int count = 0;
	
    while (str && str[count]) {
		count++;
    }
	
    return count;
}

static void
copy_string(char *dest, char *src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest++ = 0;
}

static char *
copy_string(char *src) {
    int len = string_length(src);
    if (len == 0) return 0;
    
    char *result = (char *) alloc(sizeof(char) * len);
    copy_string(result, src);
    return result;
}

static u32
count_lines(u8 *buffer) {
    u32 result = 0;
    
    u8 *at = buffer;
    
    while (1) {
        b32 reached_end = *at == '\0';
        if (*at == '\n' || *at == '\0') {
            result++;
        }
        if (reached_end) break;
        at++;
    }
    
    return result;
}

inline bool32
is_eof(char c) {
    bool32 result = ((c == '\n') ||
                     (c == '\r'));
    
    return result;
}

inline bool32
is_whitespace(char c) {
    bool32 result = ((c == ' ') ||
                     (c == '\t') ||
                     (c == '\v') ||
                     (c == '\f') ||
                     is_eof(c));
    
    return result;
}

static void
eat_spaces(u8 **a) {
    if (!a) return;
    
    char *at = (char *) *a;
    while (is_whitespace(at[0])) {
        (*a)++; at++;
    }
}

static void
init_handler(Text_File_Handler *handler, u8 *buffer) {
    handler->data = buffer;
    handler->current_line = 0;
    handler->num_lines = count_lines(buffer);
}

static char *
consume_next_line(u8 **buffer) {
    if (!buffer) return 0;
    
    eat_spaces(buffer);
    
    u8 *at = *buffer;
    if (*at == '\0') {
        return 0;
    }
    
    char *line = (char *) at;
    
    int count = 0;
    while (*at++) {
        if (*at == '\n' || *at == '\r' || *at == '\0') {
            line = (char*) *buffer;
            line[++count] = '\0';
            break;
        }
        ++count;
    }
    
    *buffer += count + 1;
    
    return line;
}

static char *
consume_next_line(Text_File_Handler *handler) {
    if (!handler) return 0;
    
    while (handler->current_line < handler->num_lines) {
        handler->current_line++;
        char *line = consume_next_line(&handler->data);
        if (!line) {
            continue;
        }
        return line; 
    }
    
    return 0;
}

static int
find_character_from_left(char *a, char c) {
    int result = -1;
    int i = 0;
    while (*a) {
        if (*a == c) {
            result = i;
            break;
        }
        *a++; i++;
    }
    return result;
}

static Break_String_Result
break_by_tok(char *a, char tok) {
    Break_String_Result result = {};
    
    if (!a) {
        result.lhs = 0;
        result.rhs = 0;
    } else {
        int pos = find_character_from_left(a, tok);
        if (pos >= 0) {
            char *lhs = a; lhs[pos++] = '\0';
            result.lhs = lhs;
            result.rhs = a + pos;
        } else {
            result.lhs = a;
            result.rhs = 0;
        }
    }
    
    return result;
}

static Break_String_Result
break_by_spaces(char *a) {
    Break_String_Result result = {};
    
    result = break_by_tok(a, ' ');
    
    return result;
}

static Break_String_Result
break_at_index(char *a, int index) {
    Break_String_Result result = {};
    
    if (!a) {
        result.lhs = 0;
        result.rhs = 0;
    } else {
        if (index > 0) {
            char *lhs = a; lhs[index++] = '\0';
            result.lhs = lhs;
            result.rhs = a + index;
        }
    }
    
    return result;
}
