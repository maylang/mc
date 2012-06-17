#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"

typedef struct Tydefn Tydefn;
struct Tydefn {
    int line;
    Node *name;
    Type *type;
};

#define Maxstabdepth 128
static Stab *stabstk[Maxstabdepth];
static int stabstkoff;
Stab *curstab()
{
    return stabstk[stabstkoff - 1];
}

void pushstab(Stab *st)
{
    stabstk[stabstkoff++] = st;
}

void popstab(void)
{
    stabstkoff--;
}

static ulong namehash(void *n)
{
    return strhash(namestr(n));
}

static int nameeq(void *a, void *b)
{
    return a == b || !strcmp(namestr(a), namestr(b));
}

Stab *mkstab()
{
    Stab *st;

    st = zalloc(sizeof(Stab));
    st->ns = mkht(namehash, nameeq);
    st->dcl = mkht(namehash, nameeq);
    st->ty = mkht(namehash, nameeq);
    return st;
}

/* FIXME: do namespaces */
Node *getdcl(Stab *st, Node *n)
{
    Node *s;
    Stab *orig;

    orig = st;
    do {
        if ((s = htget(st->dcl, n))) {
            /* record that this is in the closure of this scope */
            if (!st->closure)
                st->closure = mkht(namehash, nameeq);
            if (st != orig)
                htput(st->closure, s->decl.name, s);
            return s;
        }
        st = st->super;
    } while (st);
    return NULL;
}

Type *gettype(Stab *st, Node *n)
{
    Tydefn *t;
    
    do {
        if ((t = htget(st->ty, n)))
            return t->type;
        st = st->super;
    } while (st);
    return NULL;
}

Stab *getns(Stab *st, Node *n)
{
    Stab *s;
    do {
        if ((s = htget(st->ns, n)))
            return s;
        st = st->super;
    } while (st);
    return NULL;
}

void putdcl(Stab *st, Node *s)
{
    Node *d;

    d = getdcl(st, s->decl.name);
    if (d)
        fatal(s->line, "%s already declared (line %d", namestr(s->decl.name), d->line);
    if (st->name)
        setns(s->decl.name, namestr(st->name));
    htput(st->dcl, s->decl.name, s);
}

void updatetype(Stab *st, Node *n, Type *t)
{
    Tydefn *td;

    td = htget(st->ty, n);
    if (!td)
        die("No type %s to update", namestr(n));
    td->type = t;
}

void puttype(Stab *st, Node *n, Type *t)
{
    Type *ty;
    Tydefn *td;

    ty = gettype(st, n);
    if (ty)
        fatal(n->line, "Type %s already defined", namestr(n));
    td = xalloc(sizeof(Tydefn));
    td->line = n->line;
    td->name = n;
    td->type = t;
    htput(st->ty, td->name, td);
}

void putns(Stab *st, Stab *scope)
{
    Stab *s;

    s = getns(st, scope->name);
    if (s)
        fatal(scope->name->line, "Ns %s already defined", namestr(s->name));
    htput(st->ns, scope->name, scope);
}

void updatens(Stab *st, char *name)
{
    void **k;
    size_t i, nk;

    if (st->name)
        die("Stab %s already has namespace; Can't set to %s", namestr(st->name), name);
    st->name = mkname(-1, name);
    k = htkeys(st->dcl, &nk);
    for (i = 0; i < nk; i++)
        setns(k[i], name);
    free(k);
    k = htkeys(st->ty, &nk);
    for (i = 0; i < nk; i++)
        setns(k[i], name);
    free(k);
    k = htkeys(st->ns, &nk);
    for (i = 0; i < nk; i++)
        setns(k[i], name);
    free(k);
}
