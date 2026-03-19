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

#define _COMPONENT_ "pib"

#include <logging/logging.h>

#include <stdlib.h>
#include <string.h>

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
    p->routemaps = malloc(p->acls_capacity * sizeof(routemap_t));

    return p;
}

void
pib_destroy(pib_t *pib)
{
    for (size_t i = 0; i < pib->acls_size; i++) {
        for (size_t j = 0; i < pib->acls[j].entries_size; j++)
            free(pib->acls[i].entries[j].expression);
        free(pib->acls[i].entries);
        free(pib->acls[i].name);
    }
    free(pib->acls);

    for (size_t i = 0; i < pib->routemaps_size; i++) {
        for (size_t j = 0; i < pib->routemaps[j].setters_size; j++) {
            if (pib->routemaps[i].setters[j].valstr1)
                free(pib->routemaps[i].setters[j].valstr1);
            if (pib->routemaps[i].setters[j].valstr2)
                free(pib->routemaps[i].setters[j].valstr2);
        }
        free(pib->routemaps[i].matchers);
        free(pib->routemaps[i].setters);
        free(pib->routemaps[i].name);
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
    if (pib->routemaps_capacity < pib->acls_size + 1) {
        pib->routemaps_capacity *= 2;
        pib->routemaps = realloc(pib->acls,
            pib->acls_capacity * sizeof(routemap_t));
    }

    routemap_t *r = &pib->routemaps[pib->routemaps_size++];

    r->name = strdup(name);
    r->deny = deny;

    r->matchers_capacity = INIT_VEC_CAPACITY;
    r->matchers_size = 0;
    r->matchers = malloc(r->matchers_capacity * sizeof(acl_t*));

    r->setters_capacity = INIT_VEC_CAPACITY;
    r->setters_size = 0;
    r->setters = malloc(r->setters_capacity * sizeof(routemap_setter_t));

    return r;
}

acl_t *
pib_acl_find(pib_t *pib, const char *name)
{
    for (size_t i = 0; i < pib->acls_size; i++)
        if (strcmp(pib->acls[i].name, name) == 0)
            return &pib->acls[i];
    return NULL;
}

routemap_t *
pib_routemap_find(pib_t *pib, const char *name)
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
acl_find(acl_t *acl, const char *expression)
{
    for (size_t i = 0; i < acl->entries_size; i++)
        if (strcmp(acl->entries[i].expression, expression) == 0)
            return &acl->entries[i];
    return NULL;
}

void
routemap_matcher_insert(routemap_t *routemap, int af, acl_t *acl)
{
    if (routemap->matchers_capacity < routemap->matchers_size + 1) {
        routemap->matchers_capacity *= 2;
        routemap->matchers = realloc(routemap->matchers,
            routemap->matchers_capacity * sizeof(acl_t*));
    }

    routemap_matcher_t *m = &routemap->matchers[routemap->matchers_size++];

    m->af = af;
    m->acl= acl;
}

routemap_matcher_t *
routemap_matcher_find(routemap_t *routemap, int af, const acl_t *acl)
{
    for (size_t i = 0; i < routemap->matchers_size; i++)
        if (routemap->matchers[i].af == af && routemap->matchers[i].acl == acl)
            return &routemap->matchers[i];
    return NULL;
}

void
routemap_setter_insert(routemap_t *routemap, const routemap_setter_t *setter)
{
    if (routemap->setters_capacity < routemap->setters_size + 1) {
        routemap->setters_capacity *= 2;
        routemap->setters = realloc(routemap->setters,
            routemap->setters_capacity * sizeof(routemap_setter_t));
    }

    routemap_setter_t *s = &routemap->setters[routemap->setters_size++];

    memcpy(s, setter, sizeof(routemap_setter_t));
}

routemap_setter_t *
routemap_setter_find(routemap_t *routemap, routemap_set_attr_t attribute)
{
    for (size_t i = 0; i < routemap->setters_size; i++)
        if (routemap->setters[i].attribute == attribute)
            return &routemap->setters[i];
    return NULL;
}

