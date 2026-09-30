/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

#ifndef CARROM_STRIKER_PATH_H
#define CARROM_STRIKER_PATH_H

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Distance the striker's centre must keep from every coin's centre while it slides back to the next player */
#define STRIKER_PATH_CLEAR (STRIKER_RADIUS_NORM + PIECE_RADIUS_NORM + 0.006f)

/* Shortest route for the striker from `from` (the pocket it fell into) to `to` (its place on the next player's baseline) that
 * never comes within STRIKER_PATH_CLEAR of a coin, never goes near another pocket, and stays on the board. A visibility graph over
 * points around the coins, searched with Dijkstra. Writes at most `max_out` points to `out` (first is `from`, last is `to`) and
 * returns how many; a route that does not exist (or does not fit) gives the straight line, 2 points. */
int striker_path_plan(Vec2 from, Vec2 to, const Vec2* coins, int coin_count, Vec2* out, int max_out);

/* Total length of a path */
float striker_path_length(const Vec2* pts, int n);

#ifdef __cplusplus
}
#endif

#endif
