/*
 *                This source code is part of
 * 
 *                     E  R  K  A  L  E
 *                             -
 *                       DFT from Hel
 *
 * Written by Jussi Lehtola, 2010-2011
 * Copyright (c) 2010-2011, Jussi Lehtola
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "casida_grid.h"

CasidaAtom::CasidaAtom() {
}

CasidaAtom::CasidaAtom(const BasisSet & bas, const arma::mat & P, size_t cenind, double toler, int x_func, int c_func, bool lobatto, bool verbose) : AtomGrid(bas,P,cenind,toler,x_func,c_func,lobatto,verbose) {
}

CasidaAtom::CasidaAtom(const BasisSet & bas, const arma::mat & Pa, const arma::mat & Pb, size_t cenind, double toler, int x_func, int c_func, bool lobatto, bool verbose) : AtomGrid(bas,Pa,Pb,cenind,toler,x_func,c_func,lobatto,verbose) {
}

CasidaAtom::~CasidaAtom() {
}

void CasidaAtom::compute_orbs(const std::vector<arma::mat> & C) {
  if(C.size()==1 && polarized) {
    ERROR_INFO();
    throw std::runtime_error("Trying to calculate restricted orbitals with unrestricted grid.\n");
  } else if(C.size()>1 && !polarized) {
    ERROR_INFO();
    throw std::runtime_error("Trying to calculate unrestricted orbitals with restricted grid.\n");
  }
  
  orbs.resize(C.size());
  for(size_t ispin=0;ispin<C.size();ispin++)
    // Resize to number of grid points
    orbs[ispin].resize(grid.size());

  // Loop over grid points
  for(size_t ip=0;ip<grid.size();ip++) {

    // Initialize orbital values
    for(size_t ispin=0;ispin<C.size();ispin++) {
      orbs[ispin][ip].resize(C[ispin].n_cols);
      for(size_t io=0;io<C[ispin].n_cols;io++)
	orbs[ispin][ip][io]=0.0;
    }

    // Loop over functions on grid point
    size_t first=grid[ip].f0;
    size_t last=first+grid[ip].nf;

    for(size_t ispin=0;ispin<C.size();ispin++)
      for(size_t ii=first;ii<last;ii++) {
	// Increment values of orbitals
	for(size_t io=0;io<C[ispin].n_cols;io++)
	  orbs[ispin][ip][io]+=C[ispin](flist[ii].ind,io)*flist[ii].f;
      }
  }
}
    
void CasidaAtom::eval_fxc(int x_func, int c_func) {
  int nspin;
  if(!polarized)
    nspin=XC_UNPOLARIZED;
  else
    nspin=XC_POLARIZED;

  // Allocate memory for fx and fc
  if(!polarized) {
    if(fx.size()!=grid.size())
      fx.resize(grid.size());
    if(fc.size()!=grid.size())
      fc.resize(grid.size());
  } else {
    if(fc.size()!=3*grid.size())
      fc.resize(3*grid.size());
    if(fx.size()!=3*grid.size())
      fx.resize(3*grid.size());
  }
  
  // Correlation and exchange functionals
  xc_func_type xfunc;
  if(x_func>0) {
    if(xc_func_init(&xfunc, x_func, nspin) != 0) {
      ERROR_INFO();
      std::ostringstream oss;
      oss << "Functional "<<x_func<<" not found!"; 
      throw std::runtime_error(oss.str());
    }
    
    // Initialize the functional
    xc_func_init(&xfunc, x_func, nspin);

    if(xfunc.info->family!=XC_FAMILY_LDA) {
      ERROR_INFO();
      throw std::runtime_error("Casida only supports LDA functionals.\n");
    }

    // Evaluate fx
    xc_lda_fxc(&xfunc, grid.size(), &rho[0], &fx[0]);

    // Free the functional
    xc_func_end(&xfunc);
  } else {
    // No exchange.
    for(size_t i=0;i<fx.size();i++)
      fx[i]=0.0;
  }

  if(c_func>0) {
    xc_func_type cfunc;
    if(xc_func_init(&cfunc, c_func, nspin) != 0) {
      ERROR_INFO();
      std::ostringstream oss;
      oss << "Functional "<<c_func<<" not found!"; 
      throw std::runtime_error(oss.str());
    }
    
    xc_func_init(&cfunc, c_func, nspin);
    if(cfunc.info->family!=XC_FAMILY_LDA) {
      ERROR_INFO();
      throw std::runtime_error("Casida only supports LDA functionals.\n");
    }
    
    // Evaluate fx and fc
    xc_lda_fxc(&cfunc, grid.size(), &rho[0], &fc[0]);
    
    // Free the functionals
    xc_func_end(&cfunc);
  } else {
    // No correlation.
    for(size_t i=0;i<fc.size();i++)
      fc[i]=0.0;
  }
}

void CasidaAtom::Kxc(const std::vector< std::vector<states_pair_t> > & pairs, arma::mat & K) const {
  double wxc;

  if(polarized && pairs.size()!=2) {
    ERROR_INFO();
    throw std::runtime_error("Running with polarized grid but non-polarized pairs!\n");
  } else if(!polarized && pairs.size()==2) {
    ERROR_INFO();
    throw std::runtime_error("Running with unpolarized grid but polarized pairs!\n");
  }
  
  // Perform the integration. Loop over spins
  for(size_t ispin=0;ispin<pairs.size();ispin++)
    for(size_t jspin=0;jspin<=ispin;jspin++) {
      
      // Offset in i
      const size_t ioff=ispin*pairs[0].size();
      // Offset in j
      const size_t joff=jspin*pairs[0].size();
      
      if(ispin==jspin) {
	// Loop over grid points
	for(size_t ip=0;ip<grid.size();ip++) {
	  // Factor in common for all orbitals. First case is polarized (up-up or down-down), second case is unpolarized
	  wxc=polarized ? grid[ip].w*(fx[3*ip+2*ispin]+fc[3*ip+2*ispin]) : grid[ip].w*(fx[ip]+fc[ip]);
	  
	  // Loop over pairs
	  for(size_t ipair=0;ipair<pairs[ispin].size();ipair++) {
	    for(size_t jpair=0;jpair<ipair;jpair++) {

	      double term=wxc*orbs[ispin][ip][pairs[ispin][ipair].i]*orbs[ispin][ip][pairs[ispin][ipair].f]
		*orbs[ispin][ip][pairs[jspin][jpair].i]*orbs[ispin][ip][pairs[jspin][jpair].f];
	      K(ioff+ipair,joff+jpair)+=term;
	      K(joff+jpair,ioff+ipair)+=term;
	    }
	    K(ioff+ipair,ioff+ipair)+=wxc*orbs[ispin][ip][pairs[ispin][ipair].i]*orbs[ispin][ip][pairs[ispin][ipair].f]
	      *orbs[ispin][ip][pairs[ispin][ipair].i]*orbs[ispin][ip][pairs[ispin][ipair].f];
	  }
	}
      } else {
	// Loop over grid points
	for(size_t ip=0;ip<grid.size();ip++) {
	  // Factor in common for all orbitals
	  wxc=grid[ip].w*(fx[3*ip+1]+fc[3*ip+1]); // up-down and down-up
	  
	  // Loop over pairs
	  for(size_t ipair=0;ipair<pairs[ispin].size();ipair++) {
	    for(size_t jpair=0;jpair<ipair;jpair++) {
	      double term=wxc*orbs[ispin][ip][pairs[ispin][ipair].i]*orbs[ispin][ip][pairs[ispin][ipair].f]
		*orbs[ispin][ip][pairs[jspin][jpair].i]*orbs[ispin][ip][pairs[jspin][jpair].f];
	      K(ioff+ipair,joff+jpair)+=term;
	      K(joff+jpair,ioff+ipair)+=term;
	    }
	    K(ioff+ipair,ioff+ipair)+=wxc*orbs[ispin][ip][pairs[ispin][ipair].i]*orbs[ispin][ip][pairs[ispin][ipair].f]
	      *orbs[ispin][ip][pairs[ispin][ipair].i]*orbs[ispin][ip][pairs[ispin][ipair].f];
	  }
	}
      }  
    }
}

void CasidaAtom::free() {
  AtomGrid::free();
  orbs.clear();
  fx.clear();
  fc.clear();
}


CasidaGrid::CasidaGrid(const BasisSet * bas, bool dir, bool ver, bool lobatto) {
  basp=bas;
  direct=dir;
  verbose=ver;
  use_lobatto=lobatto;
  
  atoms.resize(basp->get_Nnuc());
}

CasidaGrid::~CasidaGrid() {
}

void CasidaGrid::construct(const std::vector<arma::mat> & P, double tol, int x_func, int c_func) {

  // Add all atoms
  if(verbose) {
    printf("\tatom\tNpoints\tNfuncs\n");
  }

  size_t Nat=basp->get_Nnuc();

  if(P.size()==1) {
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1)
#endif
    for(size_t i=0;i<Nat;i++)
      atoms[i]=CasidaAtom(*basp,P[0],i,tol,x_func,c_func,use_lobatto,verbose);
  } else {
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1)
#endif
    for(size_t i=0;i<Nat;i++)
      atoms[i]=CasidaAtom(*basp,P[0],P[1],i,tol,x_func,c_func,use_lobatto,verbose);
  }
  
  // If we are not running a direct calculation, compute grids and basis functions.
  if(!direct)
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1)
#endif
    for(size_t i=0;i<Nat;i++) {
      atoms[i].form_grid(*basp);
      atoms[i].compute_bf(*basp);
    }
}

void CasidaGrid::Kxc(const std::vector<arma::mat> & P, double tol, int x_func, int c_func, const std::vector<arma::mat> & C, const std::vector< std::vector<states_pair_t> > & pairs, arma::mat & Kx) {
  // First, we need to construct the grid.
  construct(P,tol,x_func,c_func);
  
  /* Consistency checks */
  if(P.size()!=C.size()) {
    ERROR_INFO();
    throw std::runtime_error("Sizes of P and C are inconsistent!\n");
  }
  if(P.size()!=pairs.size()) {
    ERROR_INFO();
    throw std::runtime_error("Sizes of P and pairs are inconsistent!\n");
  }

  // Now, loop over the atoms.
  for(size_t i=0;i<atoms.size();i++) {
    // Compute functions if necessary.
    if(direct) {
      // Form grid
      atoms[i].form_grid(*basp);
      // Compute values of basis functions
      atoms[i].compute_bf(*basp);
    }

    // Compute the values of the orbitals
    atoms[i].compute_orbs(C);

    // Update the density
    if(P.size()==1)
      atoms[i].update_density(P[0]);
    else 
      atoms[i].update_density(P[0],P[1]);
    // and compute fxc
    atoms[i].eval_fxc(x_func,c_func);

    // Compute Kxc's
    atoms[i].Kxc(pairs, Kx);
    
    // Free the memory
    if(direct)
      atoms[i].free();
  }

  // Symmetrize K if necessary
  for(size_t ispin=0;ispin<pairs.size();ispin++)
    for(size_t jspin=0;jspin<=ispin;jspin++) {
      // Offset in i
      const size_t ioff=ispin*pairs[0].size();
      // Offset in j
      const size_t joff=jspin*pairs[0].size();
      
      if(ispin!=jspin) {
	Kx.submat(joff,ioff,joff+pairs[jspin].size()-1,ioff+pairs[ispin].size()-1)=arma::trans(Kx.submat(ioff,joff,ioff+pairs[ispin].size()-1,joff+pairs[jspin].size()-1));
      }
    }
}
