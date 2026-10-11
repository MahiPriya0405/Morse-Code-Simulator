#ifndef MORSE_LOGIC_H
#define MORSE_LOGIC_H

/* All four functions return a heap-allocated string.
   The caller is responsible for calling free() on the result. */

char *text_to_morse(const char *input);
char *number_to_morse(const char *input);
char *morse_to_text(const char *input);
char *morse_to_number(const char *input);

#endif
