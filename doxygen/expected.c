/**
file doxygen comment. Block comment with custom banner consisting of dashes.
Similar to what Postgres uses.
  - this single banner character should not be stripped,
  - but two-or-more in a row (eg."") will be stripped.
 @file example.c */

/*********************************************************************************
 * leading docgen comment. This is a more typical Javadoc banner style.
 * Does doxygen add it to the file comment or to the struct comment?
 *********************************************************************************/

/// leading doxygen comment. This comment will only apply to the first forward reference.
/// leading doxygen comment. Should we group adjacent statements together so this comment applies to both?
struct forward; ///<  trailing doxygen comment.
void proc(void); ///<  trailing doxygen comment.

#include <stdio.h>
#include "example.h"

/** leading doxygen comment. A macro which does nothing. */
#define DUMMY_MACRO 0

/// leading doxygen comment.  Multiline define.
#define MultiLine  \
     {{{{{{{{{((((((( This could really mess up the level of braces if it were parsed. \
     but of course we skip over it so we should be OK.

int value = 0;  ///<  trailing doxygen comment.

/// leading doxygen comment. We want to document the something field.
typedef struct Foo {
    int something;  ///<  trailing doxygen comment.
} Foo; ///<  trailing doxygen comment.

/// leading doxygen comment. Describe the main function
int main()
{  // NOT a doxygen comment.

    printf("Hello, World!\n"); // NOT a doxygen comment.

    int a = 5; int b = 6;  // NOT a doxygen comment. Multi statements on single line.

    /* NOT a doxygen comment. */
    return 0;

} ///<  trailing doxygen comment.


/** Leading doxygen comment. */
struct XXX *proc(struct XXX *bar) {
    int xxx;  // Not a doxygen comment.
    struct YYY {
        /* Not a doxygen comment. Doxygen only used for globals. */
        int x;
    }; // Not a doxygen comment. */
} ///<  Trailing doxygen comment.




