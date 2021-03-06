/**************************************************************************
 * Flux-coordinate Independent parallel derivatives
 *
 **************************************************************************
 * Copyright 2014 B.D.Dudson, P. Hill
 *
 * Contact: Ben Dudson, bd512@york.ac.uk
 *
 * This file is part of BOUT++.
 *
 * BOUT++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BOUT++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.
 *
 **************************************************************************/

#ifndef __FCITRANSFORM_H__
#define __FCITRANSFORM_H__

#include <bout/paralleltransform.hxx>
#include <interpolation.hxx>
#include <mask.hxx>
#include <parallel_boundary_region.hxx>

/*!
 * Field line map - contains the coefficients for interpolation
 */
class FCIMap {
  /// Interpolation object
  Interpolation *interp;

  /// Private constructor - must be initialised with mesh
  FCIMap();
public:
  /// dir MUST be either +1 or -1
  FCIMap(Mesh& mesh, int dir, bool yperiodic, bool zperiodic);

  int dir;                     /**< Direction of map */

  BoutMask boundary_mask;      /**< boundary mask - has the field line left the domain */
  Field3D y_prime;             /**< distance to intersection with boundary */

  BoundaryRegionPar* boundary; /**< boundary region */

  const Field3D interpolate(Field3D &f) const { return interp->interpolate(f); }
};

/*!
 * Flux Coordinate Independent method for parallel derivatives
 */
class FCITransform : public ParallelTransform {
public:
  FCITransform(Mesh& mesh, bool yperiodic=true, bool zperiodic=true) :
    mesh(mesh),
    forward_map(mesh, +1, yperiodic, zperiodic),
    backward_map(mesh, -1, yperiodic, zperiodic),
    yperiodic(yperiodic),
    zperiodic(zperiodic) {}

  void calcYUpDown(Field3D &f);

  const Field3D toFieldAligned(const Field3D &UNUSED(f)) {
    throw BoutException("FCI method cannot transform into field aligned grid");
  }

  const Field3D fromFieldAligned(const Field3D &UNUSED(f)) {
    throw BoutException("FCI method cannot transform into field aligned grid");
  }
private:
  FCITransform();

  Mesh& mesh;

  FCIMap forward_map;           /**< FCI map for field lines in +ve y */
  FCIMap backward_map;          /**< FCI map for field lines in -ve y */

  bool yperiodic;               /**< Is the y-direction periodic? */
  bool zperiodic;               /**< Is the z-direction periodic? */
};

#endif // __FCITRANSFORM_H__
