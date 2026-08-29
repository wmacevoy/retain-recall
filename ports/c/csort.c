/*
  Sorting two different arrays through one unmodified C sort.
  ===========================================================

  include/sort.h declares compare() and swap() as plain globals that the
  caller must define.  There is no context parameter, so the implementation
  has to name a specific array: src/oldsort.c names `a`, and that program
  can therefore sort exactly one array, of one element type, forever.

  src/newsort.cpp escapes that with retain/recall in C++.  This is the same
  escape without leaving C.  src/sort.c is not touched, sort.h is not
  touched, and the program below sorts a double array and a string array --
  and nests one sort inside another.

  The object model is the oopc pattern (../oopc): one const vtable per
  class, one vtable pointer per object, prefix layout so upcasting is a
  plain pointer cast, and a Type carrying name/parent/size for a checked
  downcast.  Its three rules are followed here:

    1. Every vtable slot takes the ROOT type (Sortable *) at every level, so
       a derived class inherits a slot by naming the same function pointer
       and no function-pointer cast ever arises.  Casting a function pointer
       to another signature and calling through it is undefined (C11
       6.5.2.2p9) and traps under CFI.  Implementations downcast their
       Sortable * argument instead, which is an ordinary data-pointer cast
       and exactly what prefix layout is for.
    2. Tables are static const with compile-time initializers, so they live
       in read-only memory and nothing is built lazily.
    3. Every vtable begins with its Type, so any object reports its class
       through the vtable it already points at, at no per-object cost.

  A real project would link oopc's Type.c rather than restate it; the few
  lines here keep this file to one translation unit.
*/

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "sort.h"
#include "retain.h"

/*
 * runtime types (oopc include/Type.h, reduced to one hierarchy's worth)
 */

typedef struct TypeStruct Type;
struct TypeStruct {
    const char *name;
    const Type *parent;   /* 0 at the root */
    size_t size;
};

typedef struct { const Type *type; } AnyVftbl;
typedef struct { const AnyVftbl *vftbl; } AnyObject;

static const Type *Type_of(const void *object)
{
    if (object == NULL) return NULL;
    return ((const AnyObject *)object)->vftbl->type;
}

static int Type_isa(const Type *type, const Type *base)
{
    for (; type != NULL; type = type->parent) {
        if (type == base) return 1;
    }
    return 0;
}

static void *Type_cast(void *object, const Type *base)
{
    return Type_isa(Type_of(object), base) ? object : NULL;
}

/*
 * Sortable -- the root class
 */

typedef struct SortableStruct Sortable;

typedef struct {
    const Type *type;
    size_t (*size)(const Sortable *me);
    int (*compare)(const Sortable *me, size_t i, size_t j);
    void (*swap)(Sortable *me, size_t i, size_t j);
} SortableVftbl;

struct SortableStruct {
    const SortableVftbl *vftbl;
};

static const Type Sortable_type = { "Sortable", NULL, sizeof(Sortable) };

/* Dispatchers: call these, not the slots. */
static size_t Sortable_size(const Sortable *me)
{
    return me->vftbl->size(me);
}

static int Sortable_compare(const Sortable *me, size_t i, size_t j)
{
    return me->vftbl->compare(me, i, j);
}

static void Sortable_swap(Sortable *me, size_t i, size_t j)
{
    me->vftbl->swap(me, i, j);
}

/* Checked downcast -- 0 if `object` is not a Sortable or anything derived. */
static Sortable *Sortable_cast(void *object)
{
    return (Sortable *)Type_cast(object, &Sortable_type);
}

/*
 * Doubles -- a Sortable view of a double array
 */

typedef struct {
    const SortableVftbl *vftbl;   /* prefix layout: Sortable's fields, repeated */
    double *a;
    size_t n;
} Doubles;

static size_t Doubles_size(const Sortable *me)
{
    return ((const Doubles *)me)->n;
}

static int Doubles_compare(const Sortable *me, size_t i, size_t j)
{
    const Doubles *self = (const Doubles *)me;
    if (self->a[i] < self->a[j]) return -1;
    if (self->a[j] < self->a[i]) return  1;
    return 0;
}

static void Doubles_swap(Sortable *me, size_t i, size_t j)
{
    Doubles *self = (Doubles *)me;
    double tmp = self->a[i];
    self->a[i] = self->a[j];
    self->a[j] = tmp;
}

static const Type Doubles_type = { "Doubles", &Sortable_type, sizeof(Doubles) };

static const SortableVftbl Doubles_vftbl = {
    &Doubles_type, Doubles_size, Doubles_compare, Doubles_swap
};

static void Doubles_init(Doubles *me, double *a, size_t n)
{
    me->vftbl = &Doubles_vftbl;
    me->a = a;
    me->n = n;
}

/*
 * Strings -- a Sortable view of a char* array
 */

typedef struct {
    const SortableVftbl *vftbl;
    const char **a;
    size_t n;
} Strings;

static size_t Strings_size(const Sortable *me)
{
    return ((const Strings *)me)->n;
}

static int Strings_compare(const Sortable *me, size_t i, size_t j)
{
    const Strings *self = (const Strings *)me;
    return strcmp(self->a[i], self->a[j]);
}

static void Strings_swap(Sortable *me, size_t i, size_t j)
{
    Strings *self = (Strings *)me;
    const char *tmp = self->a[i];
    self->a[i] = self->a[j];
    self->a[j] = tmp;
}

static const Type Strings_type = { "Strings", &Sortable_type, sizeof(Strings) };

static const SortableVftbl Strings_vftbl = {
    &Strings_type, Strings_size, Strings_compare, Strings_swap
};

static void Strings_init(Strings *me, const char **a, size_t n)
{
    me->vftbl = &Strings_vftbl;
    me->a = a;
    me->n = n;
}

/*
 * the bridge
 */

RETAIN_DECLARE(Sortable);
RETAIN_DEFINE(Sortable);

/*
  These are the globals sort.h asks for.  They take no context, which is the
  whole problem; recall supplies it from the call chain instead.  Compare
  with src/oldsort.c, where the same two functions have to name one array.
*/
int compare(size_t i, size_t j)
{
    return Sortable_compare(RECALL(Sortable), i, j);
}

void swap(size_t i, size_t j)
{
    Sortable_swap(RECALL(Sortable), i, j);
}

/* Sort anything Sortable with the C sort that cannot take a context. */
static void sort_sortable(Sortable *s)
{
    RETAIN(Sortable, s);
    sort(0, Sortable_size(s));
}

/*
  A sort running inside another sort's retain.  A global could not survive
  this: the inner sort would overwrite the outer's context and the outer
  would resume against the wrong array.  The stack restores it.
*/
static void sort_nested(Sortable *outer, Sortable *inner)
{
    RETAIN(Sortable, outer);
    sort_sortable(inner);
    assert(RECALL(Sortable) == outer && "outer context restored");
    sort(0, Sortable_size(outer));
}

/*
 * use
 */

int main(void)
{
    double values[] = { 1.1, -2.2, 3.0, -1.0, 2.5 };
    const char *words[] = { "caterwaul", "cat", "caterpillar", "catalog" };
    size_t i;

    Doubles doubles;
    Strings strings;

    Doubles_init(&doubles, values, sizeof(values) / sizeof(values[0]));
    Strings_init(&strings, words, sizeof(words) / sizeof(words[0]));

    assert(!RETAINED(Sortable) && "nothing retained yet");

    /* two different arrays, two element types, one unmodified sort.c */
    sort_nested((Sortable *)&doubles, (Sortable *)&strings);

    assert(!RETAINED(Sortable) && "both retains popped");

    for (i = 1; i < doubles.n; ++i) assert(values[i - 1] <= values[i]);
    for (i = 1; i < strings.n; ++i) assert(strcmp(words[i - 1], words[i]) <= 0);

    /* the checked downcast the vtable's Type buys */
    assert(Sortable_cast(&doubles) == (Sortable *)&doubles);
    assert(Sortable_cast(&strings) == (Sortable *)&strings);
    assert(Type_of(&doubles) == &Doubles_type);
    assert(strcmp(Type_of(&strings)->name, "Strings") == 0);

    printf("doubles:");
    for (i = 0; i < doubles.n; ++i) printf(" %g", values[i]);
    printf("\nstrings:");
    for (i = 0; i < strings.n; ++i) printf(" %s", words[i]);
    printf("\nsorted %s and %s through one sort.c\n",
           Type_of(&doubles)->name, Type_of(&strings)->name);

    return 0;
}
