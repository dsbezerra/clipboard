struct Text_File_Handler {
    u8 *data;
    u32 current_line;
    u32 num_lines;
};

struct Break_String_Result {
    char *lhs;
    char *rhs;
};
