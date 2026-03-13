<!--!
\defgroup command Decision Process
\ingroup docs
\hidegroupgraph
[TOC]
-->

# Decision Process

## Phase 1: Calculation of Degree of Preference

Occurs upon reception of UPDATE. Adj-TRIB-in is updated with computed
`local_pref`.

Can be peer-paralleled

 - For internal routes, it takes LocalPref value
 - For external routes, its locally determined by means of configuration

## Phase 2: Route Selection

Occurs on TRIB update

### Phase 2a: External routes and local routes

Route selection alg (external Ext-TRIBs-in, local route table) -> Ext-TRIB

### Phase 2b: Ext-TRIB and internal routes

Route selection alg (internal Ext-TRIBs-in, Ext-TRIB) -> Loc-TRIB

### Route selection

#### Overlapping routes

More specific route takes precedence

Both routes will be installed in tripd implementation

#### Breaking ties (route selection)

In order of selection

 - Higher degree of preference
    - LocalPref if internal, locally configured preference if external
 - Higher MultiExitDisc
 - Internal route with lowest peer ID
 - External route with lowest ITAD
 - External route with lowest ID

## Phase 3: Route disemination

After Loc-TRIB is changed or new session is established.

All routes in Loc-TRIB are processed into Adj-TRIBs-out optionally applying
aggregation or reductions.

## Update processes

### Internal update process

Upon reception of UPDATE, flood other internal peers

Send all Ext-TRIB to peer, with itself as Originator

Remove withdrawn routes from Ext-TRIB-in

If some withdrawn routes were in Ext-TRIB, insert replacement route if available
or mark withdrawn in Ext-TRIB-out and UPDATE

### External update process

Phase 3 has updated Adj-TRIBs-out.

Send UPDATE messages with new routes in ReachableRoutes, and those marked as
withdrawned or newly unreachable routes within ITAD in WithdrawnRoutes.

