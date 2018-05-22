/*
 * Implements the shifted metric method for parallel derivatives
 * 
 * By default fields are stored so that X-Z are orthogonal,
 * and so not aligned in Y.
 *
 */

#include <bout/paralleltransform.hxx>
#include <bout/mesh.hxx>
#include <interpolation.hxx>
#include <fft.hxx>
#include <bout/constants.hxx>

#include <cmath>

#include <output.hxx>

ShiftedMetric::ShiftedMetric(Mesh &m) : mesh(m), zShift(&m) {
  // Read the zShift angle from the mesh
  
  if(mesh.get(zShift, "zShift")) {
    // No zShift variable. Try qinty in BOUT grid files
    mesh.get(zShift, "qinty");
  }

  if (mesh.xstart >=2) {
    // Can interpolate in x-direction
    // Calculate staggered field for zShift and apply boundary conditions
    Field2D zShift_XLOW = interp_to(zShift, CELL_XLOW, RGN_ALL);
    zShift_XLOW.applyBoundary("neumann"); // Set boundary guard cells to closest grid cell value
    zShift.set(zShift_XLOW);
  }
  if (mesh.ystart >=2) {
    // Can interpolate in y-direction
    // Calculate staggered field for zShift and apply boundary conditions
    Field2D zShift_YLOW = interp_to(zShift, CELL_YLOW, RGN_ALL);
    zShift_YLOW.applyBoundary("neumann"); // Set boundary guard cells to closest grid cell value
    zShift.set(zShift_YLOW);
  }

  int nmodes = mesh.LocalNz/2 + 1;
  //Allocate storage for complex intermediate
  cmplx = Array<dcomplex>(nmodes);
  std::fill(cmplx.begin(), cmplx.end(), 0.0);
}

//As we're attached to a mesh we can expect the z direction to not change
//once we've been created so cache the complex phases used in transformations
//the first time they are needed
Matrix< Array<dcomplex> > ShiftedMetric::getFromAlignedPhs(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      fromAlignedPhs_CENTRE = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : fromAlignedPhs_CENTRE) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=0;jy<mesh.LocalNy;jy++) {
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            fromAlignedPhs_CENTRE(jx, jy)[jz] = dcomplex(cos(kwave*zShift(jx,jy)) , -sin(kwave*zShift(jx,jy)));
          }
        }
      }
    }
    return fromAlignedPhs_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      ASSERT1(mesh.xstart>=2); //otherwise we cannot interpolate in the x-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at XLOW
      Field2D zShift_XLOW = zShift.get(CELL_XLOW);

      first_XLOW = false;
      fromAlignedPhs_XLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : fromAlignedPhs_XLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=0;jy<mesh.LocalNy;jy++) {
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            fromAlignedPhs_XLOW(jx, jy)[jz] = dcomplex(cos(kwave*zShift_XLOW(jx,jy)), -sin(kwave*zShift_XLOW(jx,jy)));
          }
        }
      }
    }
    return fromAlignedPhs_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      ASSERT1(mesh.ystart>=2); //otherwise we cannot interpolate in the y-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at YLOW
      Field2D zShift_YLOW = zShift.get(CELL_YLOW);

      first_YLOW = false;
      fromAlignedPhs_YLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : fromAlignedPhs_YLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=0;jy<mesh.LocalNy;jy++) {
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            fromAlignedPhs_YLOW(jx, jy)[jz] = dcomplex(cos(kwave*zShift_YLOW(jx,jy)), -sin(kwave*zShift_YLOW(jx,jy)));
          }
        }
      }
    }
    return fromAlignedPhs_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getFromAlignedPhs(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

Matrix< Array<dcomplex> > ShiftedMetric::getToAlignedPhs(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      toAlignedPhs_CENTRE = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : toAlignedPhs_CENTRE) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=0;jy<mesh.LocalNy;jy++) {
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            toAlignedPhs_CENTRE(jx, jy)[jz] = dcomplex(cos(kwave*zShift(jx,jy)), sin(kwave*zShift(jx,jy)));
          }
        }
      }
    }
    return toAlignedPhs_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      ASSERT1(mesh.xstart>=2); //otherwise we cannot interpolate in the x-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at XLOW
      Field2D zShift_XLOW = zShift.get(CELL_XLOW);

      first_XLOW = false;
      toAlignedPhs_XLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : toAlignedPhs_XLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=0;jy<mesh.LocalNy;jy++) {
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            toAlignedPhs_XLOW(jx, jy)[jz] = dcomplex(cos(kwave*zShift_XLOW(jx,jy)), sin(kwave*zShift_XLOW(jx,jy)));
          }
        }
      }
    }
    return toAlignedPhs_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      ASSERT1(mesh.ystart>=2); //otherwise we cannot interpolate in the y-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at YLOW
      Field2D zShift_YLOW = zShift.get(CELL_YLOW);

      first_YLOW = false;
      toAlignedPhs_YLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : toAlignedPhs_YLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=0;jy<mesh.LocalNy;jy++) {
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            toAlignedPhs_YLOW(jx, jy)[jz] = dcomplex(cos(kwave*zShift_YLOW(jx,jy)), sin(kwave*zShift_YLOW(jx,jy)));
          }
        }
      }
    }
    return toAlignedPhs_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getToAlignedPhs(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

Matrix< Array<dcomplex> > ShiftedMetric::getYupPhs1(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      yupPhs1_CENTRE = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : yupPhs1_CENTRE) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal yupShift1 = zShift(jx,jy) - zShift(jx,jy+1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs1_CENTRE(jx, jy)[jz] = dcomplex(cos(kwave*yupShift1) , -sin(kwave*yupShift1));
          }
        }
      }
    }
    return yupPhs1_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      ASSERT1(mesh.xstart>=2); //otherwise we cannot interpolate in the x-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at XLOW
      Field2D zShift_XLOW = zShift.get(CELL_XLOW);

      first_XLOW = false;
      yupPhs1_XLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : yupPhs1_XLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal yupShift1 = zShift_XLOW(jx,jy) - zShift_XLOW(jx,jy+1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs1_XLOW(jx, jy)[jz] = dcomplex(cos(kwave*yupShift1) , -sin(kwave*yupShift1));
          }
        }
      }
    }
    return yupPhs1_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      ASSERT1(mesh.ystart>=2); //otherwise we cannot interpolate in the y-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at YLOW
      Field2D zShift_YLOW = zShift.get(CELL_YLOW);

      first_YLOW = false;
      yupPhs1_YLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : yupPhs1_YLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal yupShift1 = zShift_YLOW(jx,jy) - zShift_YLOW(jx,jy+1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs1_YLOW(jx, jy)[jz] = dcomplex(cos(kwave*yupShift1) , -sin(kwave*yupShift1));
          }
        }
      }
    }
    return yupPhs1_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getYupPhs1(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

Matrix< Array<dcomplex> > ShiftedMetric::getYupPhs2(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      yupPhs2_CENTRE = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : yupPhs2_CENTRE) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal yupShift2 = zShift(jx,jy) - zShift(jx,jy+2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs2_CENTRE(jx, jy)[jz] = dcomplex(cos(kwave*yupShift2) , -sin(kwave*yupShift2));
          }
        }
      }
    }
    return yupPhs2_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      ASSERT1(mesh.xstart>=2); //otherwise we cannot interpolate in the x-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at XLOW
      Field2D zShift_XLOW = zShift.get(CELL_XLOW);

      first_XLOW = false;
      yupPhs2_XLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : yupPhs2_XLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal yupShift2 = zShift_XLOW(jx,jy) - zShift_XLOW(jx,jy+2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs2_XLOW(jx, jy)[jz] = dcomplex(cos(kwave*yupShift2) , -sin(kwave*yupShift2));
          }
        }
      }
    }
    return yupPhs2_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      ASSERT1(mesh.ystart>=2); //otherwise we cannot interpolate in the y-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at YLOW
      Field2D zShift_YLOW = zShift.get(CELL_YLOW);

      first_YLOW = false;
      yupPhs2_YLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : yupPhs2_YLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal yupShift2 = zShift_YLOW(jx,jy) - zShift_YLOW(jx,jy+2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs2_YLOW(jx, jy)[jz] = dcomplex(cos(kwave*yupShift2) , -sin(kwave*yupShift2));
          }
        }
      }
    }
    return yupPhs2_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getYupPhs2(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

Matrix< Array<dcomplex> > ShiftedMetric::getYdownPhs1(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      ydownPhs1_CENTRE = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : ydownPhs1_CENTRE) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal ydownShift1 = zShift(jx,jy) - zShift(jx,jy-1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs1_CENTRE(jx, jy)[jz] = dcomplex(cos(kwave*ydownShift1) , -sin(kwave*ydownShift1));
          }
        }
      }
    }
    return ydownPhs1_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      ASSERT1(mesh.xstart>=2); //otherwise we cannot interpolate in the x-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at XLOW
      Field2D zShift_XLOW = zShift.get(CELL_XLOW);

      first_XLOW = false;
      ydownPhs1_XLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : ydownPhs1_XLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal ydownShift1 = zShift_XLOW(jx,jy) - zShift_XLOW(jx,jy-1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs1_XLOW(jx, jy)[jz] = dcomplex(cos(kwave*ydownShift1) , -sin(kwave*ydownShift1));
          }
        }
      }
    }
    return ydownPhs1_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      ASSERT1(mesh.ystart>=2); //otherwise we cannot interpolate in the y-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at YLOW
      Field2D zShift_YLOW = zShift.get(CELL_YLOW);

      first_YLOW = false;
      ydownPhs1_YLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : ydownPhs1_YLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal ydownShift1 = zShift_YLOW(jx,jy) - zShift_YLOW(jx,jy-1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs1_YLOW(jx, jy)[jz] = dcomplex(cos(kwave*ydownShift1) , -sin(kwave*ydownShift1));
          }
        }
      }
    }
    return ydownPhs1_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getYdownPhs1(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

Matrix< Array<dcomplex> > ShiftedMetric::getYdownPhs2(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      ydownPhs2_CENTRE = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : ydownPhs2_CENTRE) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal ydownShift2 = zShift(jx,jy) - zShift(jx,jy-2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs2_CENTRE(jx, jy)[jz] = dcomplex(cos(kwave*ydownShift2) , -sin(kwave*ydownShift2));
          }
        }
      }
    }
    return ydownPhs2_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      ASSERT1(mesh.xstart>=2); //otherwise we cannot interpolate in the x-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at XLOW
      Field2D zShift_XLOW = zShift.get(CELL_XLOW);

      first_XLOW = false;
      ydownPhs2_XLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : ydownPhs2_XLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal ydownShift2 = zShift_XLOW(jx,jy) - zShift_XLOW(jx,jy-2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs2_XLOW(jx, jy)[jz] = dcomplex(cos(kwave*ydownShift2) , -sin(kwave*ydownShift2));
          }
        }
      }
    }
    return ydownPhs2_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      ASSERT1(mesh.ystart>=2); //otherwise we cannot interpolate in the y-direction
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // get zShift at YLOW
      Field2D zShift_YLOW = zShift.get(CELL_YLOW);

      first_YLOW = false;
      ydownPhs2_YLOW = Matrix< Array<dcomplex> >(mesh.LocalNx, mesh.LocalNy);
      for (auto &element : ydownPhs2_YLOW) {
        element = Array<dcomplex>(mesh.LocalNz);
      }

      //To/From field aligned phases
      for(int jx=mesh.xstart; jx<=mesh.xend; jx++) {
        for(int jy=mesh.ystart; jy<=mesh.yend; jy++) {
          BoutReal ydownShift2 = zShift_YLOW(jx,jy) - zShift_YLOW(jx,jy-2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs2_YLOW(jx, jy)[jz] = dcomplex(cos(kwave*ydownShift2) , -sin(kwave*ydownShift2));
          }
        }
      }
    }
    return ydownPhs2_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getYdownPhs2(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

/*!
 * Calculate the Y up and down fields
 */
void ShiftedMetric::calcYUpDown(Field3D &f, REGION region) {
  ASSERT1(&mesh == f.getMesh());
  f.splitYupYdown();
  CELL_LOC location = f.getLocation();
  
  // We only use methods in ShiftedMetric to get fields for parallel operations
  // like interp_to or DDY.
  // Therefore we don't need x-guard cells, so do not set them.
  // (Note valgrind complains about corner guard cells if we try to loop over
  // the whole grid, because zShift is not initialized in the corner guard
  // cells.)
  // Also, only makes sense to calculate yup/ydown for the y-grid points (not
  // guard cells) since, e.g. yup(yend) contains the (shifted) value for f in
  // the 'guard cell'.
  // Therefore, only loop over RGN_NOBNDRY here.

  Field3D& yup1 = f.yup();
  yup1.allocate();
  invalidateGuards(yup1); // Won't set x-guard cells, so allow checking to throw exception if they are used.
  Matrix< Array<dcomplex> > phases = getYupPhs1(location);
  for(auto i : f.region2D(RGN_NOBNDRY)) {
    shiftZ(f(i.x, i.y+1), phases(i.x, i.y), yup1(i.x, i.y+1));
  }
  if (mesh.ystart>1) {
    Field3D& yup2 = f.yup(2);
    yup2.allocate();
    invalidateGuards(yup2); // Won't set x-guard cells, so allow checking to throw exception if they are used.
    phases = getYupPhs2(location);
    for(auto i : f.region2D(RGN_NOBNDRY)) {
      shiftZ(f(i.x, i.y+2), phases(i.x, i.y), yup2(i.x, i.y+2));
    }
  }

  Field3D& ydown1 = f.ydown();
  ydown1.allocate();
  invalidateGuards(ydown1); // Won't set x-guard cells, so allow checking to throw exception if they are used.
  phases = getYdownPhs1(location);
  for(auto i : f.region2D(RGN_NOBNDRY)) {
    shiftZ(f(i.x, i.y-1), phases(i.x, i.y), ydown1(i.x, i.y-1));
  }
  if (mesh.ystart > 1) {
    Field3D& ydown2 = f.ydown(2);
    ydown2.allocate();
    invalidateGuards(ydown2); // Won't set x-guard cells, so allow checking to throw exception if they are used.
    phases = getYdownPhs2(location);
    for(auto i : f.region2D(RGN_NOBNDRY)) {
      shiftZ(f(i.x, i.y-2), phases(i.x, i.y), ydown2(i.x, i.y-2));
    }
  }
}
  
/*!
 * Shift the field so that X-Z is not orthogonal,
 * and Y is then field aligned.
 */
const Field3D ShiftedMetric::toFieldAligned(const Field3D &f, const REGION region) {
  return shiftZ(f, getToAlignedPhs(f.getLocation()), region);
}

/*!
 * Shift back, so that X-Z is orthogonal,
 * but Y is not field aligned.
 */
const Field3D ShiftedMetric::fromFieldAligned(const Field3D &f, const REGION region) {
  return shiftZ(f, getFromAlignedPhs(f.getLocation()), region);
}

const Field3D ShiftedMetric::shiftZ(const Field3D &f, const Matrix< Array<dcomplex> > &phs, const REGION region) {
  ASSERT1(&mesh == f.getMesh());
  ASSERT1(region == RGN_NOX || region == RGN_NOBNDRY); // Never calculate x-guard cells here
  if(mesh.LocalNz == 1)
    return f; // Shifting makes no difference

  Field3D result(&mesh);
  result.setLocation(f.getLocation());
  result = 0.; // Set to value to avoid uninitialized value errors from Valgrind

  invalidateGuards(result); // Won't set x-guard cells, so allow checking to throw exception if they are used.
  
  // We only use methods in ShiftedMetric to get fields for parallel operations
  // like interp_to or DDY.
  // Therefore we don't need x-guard cells, so do not set them.
  // (Note valgrind complains about corner guard cells if we try to loop over
  // the whole grid, because zShift is not initialized in the corner guard
  // cells.)
  for(auto i : f.region2D(region)) {
    shiftZ(f(i.x, i.y), phs(i.x, i.y), result(i.x, i.y));
  }
  
  return result;

}

void ShiftedMetric::shiftZ(const BoutReal *in, const Array<dcomplex> &phs, BoutReal *out) {
  // Take forward FFT
  rfft(in, mesh.LocalNz, cmplx.begin());

  //Following is an algorithm approach to write a = a*b where a and b are
  //vectors of dcomplex.
  //  std::transform(cmplxOneOff.begin(),cmplxOneOff.end(), ptr.begin(), 
  //		 cmplxOneOff.begin(), std::multiplies<dcomplex>());

  const int nmodes = cmplx.size();
  for(int jz=1;jz<nmodes;jz++) {
    cmplx[jz] *= phs[jz];
  }

  irfft(cmplx.begin(), mesh.LocalNz, out); // Reverse FFT
}

//Old approach retained so we can still specify a general zShift
const Field3D ShiftedMetric::shiftZ(const Field3D &f, const Field2D &zangle, const REGION region) {
  ASSERT1(&mesh == f.getMesh());
  ASSERT1(region == RGN_NOX || region == RGN_NOBNDRY); // Never calculate x-guard cells here
  ASSERT1(f.getLocation() == zangle.getLocation());
  if(mesh.LocalNz == 1)
    return f; // Shifting makes no difference

  Field3D result(&mesh);
  result.allocate();
  invalidateGuards(result); // Won't set x-guard cells, so allow checking to throw exception if they are used.

  // We only use methods in ShiftedMetric to get fields for parallel operations
  // like interp_to or DDY.
  // Therefore we don't need x-guard cells, so do not set them.
  // (Note valgrind complains about corner guard cells if we try to loop over
  // the whole grid, because zShift is not initialized in the corner guard
  // cells.)
  for(auto i : f.region2D(region)) {
    shiftZ(f(i.x, i.y), mesh.LocalNz, zangle(i.x,i.y), result(i.x, i.y));
  }
  
  return result;
}

void ShiftedMetric::shiftZ(const BoutReal *in, int len, BoutReal zangle,  BoutReal *out) {
  int nmodes = len/2 + 1;

  // Complex array used for FFTs
  cmplxLoc = Array<dcomplex>(nmodes);
  
  // Take forward FFT
  rfft(in, len, cmplxLoc.begin());
  
  // Apply phase shift
  BoutReal zlength = mesh.coordinates()->zlength();
  for(int jz=1;jz<nmodes;jz++) {
    BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
    cmplxLoc[jz] *= dcomplex(cos(kwave*zangle) , -sin(kwave*zangle));
  }

  irfft(cmplxLoc.begin(), len, out); // Reverse FFT
}

void ShiftedMetric::outputVars(Datafile &file) {
  file.add(zShift, "zShift", 0);
}
