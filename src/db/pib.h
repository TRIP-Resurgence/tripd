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

*/

/** \file
 * \brief Policy Information Base
 */

#ifndef _PIB_H
#define _PIB_H

#include <stddef.h>


/** \brief Access Control List entry */
typedef struct {
    int             deny; /**< 0 = permit, nz = deny */
    char           *expression;
} acl_entry_t;

/** \brief Access Control List */
typedef struct {
    char           *name;
    acl_entry_t    *entries;
    size_t          entries_size, entries_capacity;
} acl_t;


/** \brief Attributes that can be set */
typedef enum {
    ROUTEMAP_SET_LOCALPREF, /**< int value */
    ROUTEMAP_SET_METRIC,    /**< int value */
    ROUTEMAP_SET_NEXTHOP    /**< string valstr1 af, valstr2 nexthop */
} routemap_set_attr_t;

/** \brief Set action */
typedef struct {
    routemap_set_attr_t attribute;
    int                 value;
    char               *valstr1, *valstr2;
} routemap_setter_t;

/** \brief Route map */
typedef struct {
    char               *name;
    acl_t             **matchers;
    size_t              matchers_size, matchers_capacity;
    routemap_setter_t  *setters;
    size_t              setters_size, setters_capacity;
} routemap_t;


/** \brief Policy Information Base */
typedef struct {
    acl_t          *acls;
    size_t          acls_size, acls_capacity;
    routemap_t     *routemaps;
    size_t          routemaps_size, routemaps_capacity;
} pib_t;


/** \brief Initialize PIB */
pib_t *pib_new();
/** \brief Deinitialize PIB */
void pib_destroy(pib_t *pib);
/** \brief Create and insert ACL into PIB */
acl_t *pib_acl_new(pib_t *pib, const char *name);
/** \brief Create and insert route map into PIB */
routemap_t *pib_routemap_new(pib_t *pib, const char *name);
/** \brief Find an ACL by name */
acl_t *pib_acl_find(pib_t *pib, const char *name);
/** \brief Find a route map by name */
routemap_t *pib_routemap_find(pib_t *pib, const char *name);

/** \brief Create and insert entry into ACL */
void acl_insert(acl_t *acl, int deny, const char *expression);
/** \brief Insert matcher ACL (stored in PIB) into route map */
void routemap_insert_matcher(routemap_t *routemap, acl_t *acl);
/** \brief Insert setter action (copy) into route map */
void routemap_insert_setter(routemap_t *routemap,
    const routemap_setter_t *setter);

#endif /* _PIB_H */

