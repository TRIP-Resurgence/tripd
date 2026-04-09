/*

    trip: Modern TRIP LS implementation
    Copyright (C) 2025 arf20 (Ángel Ruiz Fernandez)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    pib.c: Policy Information Base

*/

/** \file
 *
 * Policy Information Base, singleton
 */

#include "pib.h"

#define _COMPONENT_ "db"

#include <logging/logging.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INIT_VEC_CAPACITY   16

static pib_t g_pib = { 0 };

pib_t *
pib_new()
{
    pib_t *p = &g_pib;

    memset(p, 0, sizeof(pib_t));

    p->acls_capacity = INIT_VEC_CAPACITY;
    p->acls_size = 0;
    p->acls = malloc(p->acls_capacity * sizeof(acl_t));

    p->routemaps_capacity = INIT_VEC_CAPACITY;
    p->routemaps_size = 0;
    p->routemaps = malloc(p->routemaps_capacity * sizeof(routemap_t));

    return p;
}

void
pib_destroy(pib_t *pib)
{
    for (size_t i = 0; i < pib->acls_size; i++) {
        for (size_t j = 0; j < pib->acls[i].entries_size; j++)
            free(pib->acls[i].entries[j].expression);
        free(pib->acls[i].entries);
        free(pib->acls[i].name);
    }
    free(pib->acls);

    for (size_t i = 0; i < pib->routemaps_size; i++) {
        for (size_t j = 0; j < pib->routemaps[i].size; j++) {
            for (size_t k = 0; k < pib->routemaps[i].statements[j].matchers_size; k++)
                free(pib->routemaps[i].statements[j].matchers[k].acls);

            for (size_t k = 0; k < pib->routemaps[i].statements[j].actions_size; k++)
                routemap_statement_action_deinit(
                    &pib->routemaps[i].statements[j].actions[k]);
            free(pib->routemaps[i].statements[j].matchers);
            free(pib->routemaps[i].statements[j].actions);
        }
        free(pib->routemaps[i].name);
        free(pib->routemaps[i].statements);
    }
    free(pib->routemaps);
}

acl_t *
pib_acl_new(pib_t *pib, const char *name)
{
    if (pib->acls_capacity < pib->acls_size + 1) {
        pib->acls_capacity *= 2;
        pib->acls = realloc(pib->acls, pib->acls_capacity * sizeof(acl_t));
    }

    acl_t *a = &pib->acls[pib->acls_size++];

    a->name = strdup(name);

    a->entries_capacity = INIT_VEC_CAPACITY;
    a->entries_size = 0;
    a->entries = malloc(a->entries_capacity * sizeof(acl_entry_t));

    return a;
}

routemap_t *
pib_routemap_new(pib_t *pib, const char *name, int deny)
{
    if (pib->routemaps_capacity < pib->routemaps_size + 1) {
        pib->routemaps_capacity *= 2;
        pib->routemaps = realloc(pib->routemaps,
            pib->routemaps_capacity * sizeof(routemap_t));
    }

    routemap_t *r = &pib->routemaps[pib->routemaps_size++];

    r->name = strdup(name);

    r->size = 0;
    r->capacity = INIT_VEC_CAPACITY;
    r->statements = malloc(r->capacity * sizeof(routemap_statement_t));

    return r;
}

acl_t *
pib_acl_find(const pib_t *pib, const char *name)
{
    for (size_t i = 0; i < pib->acls_size; i++)
        if (strcmp(pib->acls[i].name, name) == 0)
            return &pib->acls[i];
    return NULL;
}

routemap_t *
pib_routemap_find(const pib_t *pib, const char *name)
{
    for (size_t i = 0; i < pib->routemaps_size; i++)
        if (strcmp(pib->routemaps[i].name, name) == 0)
            return &pib->routemaps[i];
    return NULL;
}


void
acl_insert(acl_t *acl, int deny, const char *expression)
{
    if (acl->entries_capacity < acl->entries_size + 1) {
        acl->entries_capacity *= 2;
        acl->entries = realloc(acl->entries,
            acl->entries_capacity * sizeof(acl_entry_t));
    }

    acl_entry_t *e = &acl->entries[acl->entries_size++];

    e->deny = deny;
    e->expression = strdup(expression);
}

acl_entry_t *
acl_find(const acl_t *acl, const char *expression)
{
    for (size_t i = 0; i < acl->entries_size; i++)
        if (strcmp(acl->entries[i].expression, expression) == 0)
            return &acl->entries[i];
    return NULL;
}

static int
ispfxdigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'E');
}

/**
 * \brief Check target against asterisk pattern
 *
 * \return 1 on match 0 on no match
 */
static int
pattern_check(const char *pat, const char *target)
{
    pat++; /* strip initiating _ */
    while (1) {
        /* both consumed - pattern matched */
        if (!*pat && !*target)
            return 1;

        /* wildcard matches end */
        if (*pat == '.' && !*target)
            return 1;

        /* consumed with leftover */
        if (!*pat || !*target)
            return 0;

        /* wildcard */
        if (*pat == '.') {
            if (ispfxdigit(*target)) {
                target++;
                continue;
            } else {
                return 0;
            }
        }

        /* digit check */
        if (ispfxdigit(*pat) && *pat != *target)
            return 0;
        if (*pat == 'X' && !isdigit(*target))
            return 0;
        if (*pat == 'Z' && !(*target >= '1' && *target <= '9'))
            return 0;
        if (*pat == 'N' && !(*target >= '2' && *target <= '9'))
            return 0;

        /* matched digit */
        target++;
        pat++;
    }
}

/**
 * \brief Check target against ACL
 *
 * \return 1 on permit, 0 on deny
 */
int
acl_check(const acl_t *acl, const char *target)
{
    for (size_t i = 0; i < acl->entries_size; i++) {
        const char *expr = acl->entries[i].expression;
        if (ispfxdigit(expr[0]) && strlen(expr) <= strlen(target)
            && strncmp(expr, target, strlen(expr)) == 0)
        {
            return !acl->entries[i].deny;
        } else if (pattern_check(expr, target)) {
            return !acl->entries[i].deny;
        }
    }

    return 0; /* default deny */
}

routemap_matcher_t *
routemap_statement_matcher_new(routemap_statement_t *statement, int af)
{
    if (statement->matchers_capacity < statement->matchers_size + 1) {
        statement->matchers_capacity *= 2;
        statement->matchers = realloc(statement->matchers,
            statement->matchers_capacity * sizeof(routemap_matcher_t));
    }

    routemap_matcher_t *m = &statement->matchers[statement->matchers_size++];

    m->af = af;
    m->size = 0;
    m->capacity = INIT_VEC_CAPACITY;
    m->acls = malloc(m->capacity * sizeof(acl_t*));

    return m;
}

void
routemap_statement_insert_action(routemap_statement_t *statement,
    const routemap_action_t *action)
{
    if (statement->actions_capacity < statement->actions_size + 1) {
        statement->actions_capacity *= 2;
        statement->actions = realloc(statement->actions,
            statement->actions_capacity * sizeof(routemap_action_t));
    }

    routemap_action_t *s = &statement->actions[statement->actions_size++];
    memcpy(s, action, sizeof(routemap_action_t));
}

void
routemap_statement_action_deinit(routemap_action_t *action)
{
    if (action->valstr1)
        free(action->valstr1);
    if (action->valstr2)
        free(action->valstr2);
}

void
routemap_matcher_insert(routemap_matcher_t *matcher, const acl_t *acl)
{
    if (matcher->capacity < matcher->size + 1) {
        matcher->capacity *= 2;
        matcher->acls = realloc(matcher->acls,
            matcher->capacity * sizeof(acl_t*));
    }

    matcher->acls[matcher->size++] = (acl_t *)acl;
}

void
routemap_matcher_deinit(routemap_matcher_t *matcher)
{
    free(matcher->acls);
}


routemap_action_t *
routemap_statement_action_find(const routemap_statement_t *statement,
    routemap_set_attr_t attribute)
{
    for (size_t i = 0; i < statement->actions_size; i++)
        if (statement->actions[i].attribute == attribute)
            return &statement->actions[i];
    return NULL;
}


routemap_statement_t *
routemap_statement_new(routemap_t *routemap, uint32_t seq, int deny)
{
    if (routemap->capacity < routemap->size + 1) {
        routemap->capacity *= 2;
        routemap->statements = realloc(routemap->statements,
            routemap->capacity * sizeof(routemap_statement_t));
    }

    /* ordered insert by seq */
    size_t i = 0;
    for (; i < routemap->size && routemap->statements[i].seq < seq; i++);

    memmove(&routemap->statements[i + 1], &routemap->statements[i],
        (routemap->size - i) * sizeof(routemap_statement_t));
    routemap->size++;


    routemap_statement_t *s = &routemap->statements[i];
    s->seq = seq;
    s->deny = deny;

    s->matchers_size = s->actions_size = 0;
    s->matchers_capacity = s->actions_capacity = INIT_VEC_CAPACITY;
    s->matchers = malloc(s->matchers_capacity * sizeof(routemap_matcher_t));
    s->actions = malloc(s->actions_capacity * sizeof(routemap_action_t));

    return s;
}

routemap_statement_t *
routemap_statement_find(routemap_t *routemap, uint32_t seq)
{
    for (size_t i = 0; i < routemap->size; i++)
        if (routemap->statements[i].seq == seq)
            return &routemap->statements[i];
    return NULL;
}

const routemap_statement_t *
routemap_match(const routemap_t *routemap, const char *route)
{
    for (size_t i = 0; i < routemap->size; i++) {
        for (size_t j = 0; j < routemap->statements[i].matchers_size; j++) {
            int t = 1;
            for (size_t k = 0; t && k < routemap->statements[i].matchers[j].size; k++)
                t &= acl_check(routemap->statements[i].matchers[j].acls[k], route);
            if (t)
                return &routemap->statements[i];
        }
    }

    return NULL;
}

