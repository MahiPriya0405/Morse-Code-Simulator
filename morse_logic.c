#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "morse_logic.h"

/* =========================================================
   ENCODE TABLE  (char -> morse code)
   Index 0-25  = A-Z
   Index 26-35 = 0-9
   This is a simple array lookup, giving O(1) encoding.
   ========================================================= */
static const char *CODE_TABLE[36] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..",
    ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.",
    "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..", /* A-Z */
    "-----", ".----", "..---", "...--", "....-",
    ".....", "-....", "--...", "---..", "----." /* 0-9 */
};

/* Matching symbol table, same index order as CODE_TABLE above. */
static const char SYMBOL_TABLE[36] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

/* =========================================================
   DECODE TREE (binary tree)
   -----------------------------------------------------------
   Classic Morse decode structure: starting at the root,
   a '.' moves to the left child, a '-' moves to the right
   child. Each node that completes a valid code stores the
   matching letter/digit in `symbol`.
   ========================================================= */
typedef struct TreeNode
{
    char symbol;           /* '\0' if this node is not a complete code */
    struct TreeNode *dot;  /* left child  ('.') */
    struct TreeNode *dash; /* right child ('-') */
} TreeNode;

static TreeNode *decodeTreeRoot = NULL;

static TreeNode *create_node(void)
{
    TreeNode *node = (TreeNode *)malloc(sizeof(TreeNode));
    node->symbol = '\0';
    node->dot = NULL;
    node->dash = NULL;
    return node;
}

/* Inserts one morse code (e.g. "-.-.") into the tree, storing
   `symbol` at the node the code leads to. */
static void insert_code(TreeNode *root, const char *code, char symbol)
{
    TreeNode *current = root;
    for (int i = 0; code[i] != '\0'; i++)
    {
        if (code[i] == '.')
        {
            if (current->dot == NULL)
                current->dot = create_node();
            current = current->dot;
        }
        else
        { /* '-' */
            if (current->dash == NULL)
                current->dash = create_node();
            current = current->dash;
        }
    }
    current->symbol = symbol;
}

/* Builds the full decode tree once, from the same CODE_TABLE
   used for encoding, so both directions always stay in sync. */
static void build_decode_tree(void)
{
    if (decodeTreeRoot != NULL)
        return; /* already built */
    decodeTreeRoot = create_node();
    for (int i = 0; i < 36; i++)
    {
        insert_code(decodeTreeRoot, CODE_TABLE[i], SYMBOL_TABLE[i]);
    }
}

/* Walks the tree for one morse "word" like "....", returns the
   decoded character, or '?' if the code isn't valid. */
static char decode_single_code(const char *code)
{
    TreeNode *current = decodeTreeRoot;
    for (int i = 0; code[i] != '\0'; i++)
    {
        if (current == NULL)
            return '?';
        if (code[i] == '.')
            current = current->dot;
        else if (code[i] == '-')
            current = current->dash;
        else
            return '?'; /* invalid character in the code */
    }
    if (current == NULL || current->symbol == '\0')
        return '?';
    return current->symbol;
}

/* =========================================================
   PUBLIC FUNCTIONS
   ========================================================= */

char *text_to_morse(const char *input)
{
    size_t len = strlen(input);
    /* Worst case: every char becomes up to 5 morse symbols + a space. */
    char *result = (char *)malloc(len * 8 + 1);
    result[0] = '\0';

    for (size_t i = 0; i < len; i++)
    {
        char c = (char)toupper((unsigned char)input[i]);

        if (c == ' ')
        {
            strcat(result, "/ ");
            continue;
        }

        int index = -1;
        if (c >= 'A' && c <= 'Z')
            index = c - 'A';
        else if (c >= '0' && c <= '9')
            index = 26 + (c - '0');

        if (index != -1)
        {
            strcat(result, CODE_TABLE[index]);
            strcat(result, " ");
        }
        /* unknown characters are skipped */
    }
    return result;
}

char *number_to_morse(const char *input)
{
    /* Numbers only need the digit portion of the same table,
       so this just reuses text_to_morse directly. */
    return text_to_morse(input);
}

char *morse_to_text(const char *input)
{
    build_decode_tree();

    char buffer[2048];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    size_t len = strlen(input);
    char *result = (char *)malloc(len + 1);
    size_t pos = 0;

    char *token = strtok(buffer, " ");
    while (token != NULL)
    {
        if (strcmp(token, "/") == 0)
        {
            result[pos++] = ' ';
        }
        else
        {
            result[pos++] = decode_single_code(token);
        }
        token = strtok(NULL, " ");
    }
    result[pos] = '\0';
    return result;
}

char *morse_to_number(const char *input)
{
    /* Same decode tree, just semantically used for digit codes. */
    return morse_to_text(input);
}
