#ifndef SPLATT_TILE_H
#define SPLATT_TILE_H

/******************************************************************************
 * INCLUDES
 *****************************************************************************/
#include "base.h"
#include "sptensor.h"

static idx_t const TILE_SIZES[] = { 1024, 1024, 2048 };


/******************************************************************************
 * PUBLIC FUNCTIONS
 *****************************************************************************/
void tt_tile(
  sptensor_t * const tt,
  idx_t * dim_perm);

#endif
