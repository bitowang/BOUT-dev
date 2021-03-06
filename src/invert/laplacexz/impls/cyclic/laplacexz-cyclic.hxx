
#include <bout/invert/laplacexz.hxx>
#include <cyclic_reduction.hxx>
#include <dcomplex.hxx>

class LaplaceXZcyclic : public LaplaceXZ {
public:
  LaplaceXZcyclic(Mesh *m, Options *options);
  ~LaplaceXZcyclic();
  
  using LaplaceXZ::setCoefs;
  void setCoefs(const Field2D &A, const Field2D &B) override;
  
  using LaplaceXZ::solve;
  Field3D solve(const Field3D &b, const Field3D &x0) override;
private:
  Mesh *mesh;   ///< The mesh this operates on, provides metrics and communication

  int xstart, xend;
  int nmode, nloc, nsys;
  dcomplex **acoef, **bcoef, **ccoef, **xcmplx, **rhscmplx, *k1d, *k1d_2;
  CyclicReduce<dcomplex> *cr; ///< Tridiagonal solver

  int inner_boundary_flags; ///< Flags to set inner boundary condition
  int outer_boundary_flags; ///< Flags to set outer boundary condition
};
