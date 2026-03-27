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
#include <stdint.h>


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

/** \brief Route map ACL matcher */
typedef struct {
    int                 af;
    acl_t             **acls;
    size_t              size, capacity;
} routemap_matcher_t;

/** \brief Route map Action */
typedef struct {
    routemap_set_attr_t attribute;
    int                 value;
    char               *valstr1, *valstr2;
} routemap_action_t;

/** \brief Route map statement */
typedef struct {
    uint32_t            seq;
    int                 deny;
    routemap_matcher_t *matchers;
    size_t              matchers_size, matchers_capacity;
    routemap_action_t  *actions;
    size_t              actions_size, actions_capacity;
} routemap_statement_t;

/** \brief Route map */
typedef struct {
    char                   *name;
    routemap_statement_t   *statements;
    size_t                  size, capacity;
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
routemap_t *pib_routemap_new(pib_t *pib, const char *name, int deny);
/** \brief Find an ACL by name */
acl_t *pib_acl_find(const pib_t *pib, const char *name);
/** \brief Find a route map by name */
routemap_t *pib_routemap_find(const pib_t *pib, const char *name);

/** \brief Create and insert entry into ACL */
void acl_insert(acl_t *acl, int deny, const char *expression);
/** \brief Find entry in ACL */
acl_entry_t *acl_find(const acl_t *acl, const char *expression);


/** \brief Allocate a matcher in a route map statement */
routemap_matcher_t *routemap_statement_matcher_new(
    routemap_statement_t *statement, int af);
/** \brief Insert action (copy) into route map */
void routemap_statement_insert_action(routemap_statement_t *statement,
    const routemap_action_t *action);
/** \brief Deinit route map action */
void routemap_statement_action_deinit(routemap_action_t *action);

/** \brief Insert ACL into route map matcher */
void routemap_matcher_insert(routemap_matcher_t *matcher, const acl_t *acl);
/** \brief Deinitialize a route map matcher */
void routemap_matcher_deinit(routemap_matcher_t *matcher);

/** \brief Find action by attribute */
routemap_action_t *routemap_statement_action_find(
    const routemap_statement_t *statement, routemap_set_attr_t attribute);

/** \brief Allocate statement in route map */
routemap_statement_t *routemap_statement_new(routemap_t *routemap, uint32_t seq,
    int deny);
/** \brief Find statement in route map by sequence number */
routemap_statement_t *routemap_statement_find(routemap_t *routemap,
    uint32_t seq);


/** \brief Match route against route map matchers
 *
 * Matchers are OR'd together, ACL's inside a matcher are AND'd together */
const routemap_statement_t *routemap_match(const routemap_t *routemap,
    const char *route);

#endif /* _PIB_H */

