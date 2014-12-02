/*
 *                This source code is part of
 *
 *                     E  R  K  A  L  E
 *                             -
 *                       DFT from Hel
 *
 * Written by Susi Lehtola, 2010-2011
 * Copyright (c) 2010-2011, Susi Lehtola
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */



#include <cfloat>
#include "linalg.h"
#include "mathf.h"
#include "stringutil.h"

void sort_eigvec(arma::vec & eigval, arma::mat & eigvec) {
  sort_eigvec_wrk<double>(eigval,eigvec);
}

void sort_eigvec(arma::vec & eigval, arma::cx_mat & eigvec) {
  sort_eigvec_wrk< std::complex<double> >(eigval,eigvec);
}

void eig_sym_ordered(arma::vec & eigval, arma::mat & eigvec, const arma::mat & X) {
  eig_sym_ordered_wrk<double>(eigval,eigvec,X);
} 

void eig_sym_ordered(arma::vec & eigval, arma::cx_mat & eigvec, const arma::cx_mat & X) {
  eig_sym_ordered_wrk< std::complex<double> >(eigval,eigvec,X);
} 


arma::mat CholeskyOrth(const arma::mat & S) {
  // Cholesky orthogonalization
  return inv(chol(S));
}

arma::mat SymmetricOrth(const arma::mat & Svec, const arma::vec & Sval) {
  // Compute inverse roots of eigenvalues
  arma::vec Sinvh(Sval);
  for(size_t i=0;i<Sinvh.n_elem;i++)
    if(Sinvh(i)>=LINTHRES)
      Sinvh(i)=1/sqrt(Sinvh(i));
    else
      Sinvh(i)=0.0;

  // Returned matrix is
  return Svec*arma::diagmat(Sinvh)*trans(Svec);
}

arma::mat SymmetricOrth(const arma::mat & S) {
  // Symmetric orthogonalization

  // Eigendecomposition of S: eigenvalues and eigenvectors
  arma::vec Sval;
  arma::mat Svec;
  eig_sym_ordered(Sval,Svec,S);

  // Compute the decomposition
  return SymmetricOrth(Svec,Sval);
}

arma::mat CanonicalOrth(const arma::mat & Svec, const arma::vec & Sval, double cutoff) {
  // Count number of eigenvalues that are above cutoff
  const size_t Nbf=Svec.n_rows;

  size_t Nlin=0;
  for(size_t i=0;i<Nbf;i++)
    if(Sval(i)>=cutoff)
      Nlin++;
  // Number of linearly dependent basis functions
  size_t Ndep=Nbf-Nlin;

  // Form returned matrix
  arma::mat Sinvh(Nbf,Nlin);
  for(size_t i=0;i<Nlin;i++)
    Sinvh.col(i)=Svec.col(Ndep+i)/sqrt(Sval(Ndep+i));

  return Sinvh;
}

arma::mat CanonicalOrth(const arma::mat & S, double cutoff) {
  // Canonical orthogonalization

  if(S.n_cols != S.n_rows) {
    ERROR_INFO();
    std::ostringstream oss;
    oss << "Cannot orthogonalize non-square matrix!\n";
    throw std::runtime_error(oss.str());
  }

  // Eigendecomposition of S: eigenvalues and eigenvectors
  arma::vec Sval;
  arma::mat Svec;

  // Compute the decomposition
  eig_sym_ordered(Sval,Svec,S);
  return CanonicalOrth(Svec,Sval,cutoff);
}

arma::mat BasOrth(const arma::mat & S, bool verbose) {
  // Symmetric if possible, otherwise canonical. Default cutoff
  const double tol=LINTHRES;

  // Eigendecomposition of S: eigenvalues and eigenvectors
  arma::vec Sval;
  arma::mat Svec;
  // Compute the decomposition
  eig_sym_ordered(Sval,Svec,S);

  if(verbose) {
    printf("Smallest eigenvalue of overlap matrix is %.2e, ratio to largest is %.2e.\n",Sval(0),Sval(0)/Sval(Sval.n_elem-1));
  }

  // Check smallest eigenvalue.
  if(Sval(0)>=tol) {
    // OK to use symmetric
    if(verbose) printf("Using symmetric orthogonalization.\n");

    return SymmetricOrth(Svec,Sval);
  } else {
    if(verbose) printf("Using canonical orthogonalization.\n");

    // Have to drop eigenvectors. Use canonical.
    return CanonicalOrth(Svec,Sval,tol);
  }
}

arma::mat BasOrth(const arma::mat & S, const Settings & set) {
  // Orthogonalize basis

  // Get wanted method
  std::string met=set.get_string("BasisOrth");
  // Verbose operation?
  bool verbose=set.get_bool("Verbose");

  if(stricmp(met,"auto")==0) {
    return BasOrth(S,verbose);
  } else if(stricmp(met,"Can")==0) {
    // Canonical orthogonalization
    double tol=LINTHRES;
    return CanonicalOrth(S,tol);
  } else if(stricmp(met,"Sym")==0) {
    // Symmetric orthogonalization
    return SymmetricOrth(S);
  } else if(stricmp(met,"Chol")==0) {
    return CholeskyOrth(S);
  } else {
    ERROR_INFO();
    std::ostringstream oss;
    oss << met << " is not a valid orthogonalization keyword.\n";
    throw std::domain_error(oss.str());
    return arma::mat();
  }
}

void S_half_invhalf(const arma::mat & S, arma::mat & Shalf, arma::mat & Sinvh, bool canonical, double cutoff) {
  if(S.n_cols != S.n_rows) {
    ERROR_INFO();
    std::ostringstream oss;
    oss << "Cannot orthogonalize non-square matrix!\n";
    throw std::runtime_error(oss.str());
  }

  // Size of basis
  const size_t Nbf=S.n_cols;

  // Eigendecomposition of S: eigenvalues and eigenvectors
  arma::vec Sval;
  arma::mat Svec;

  // Compute the decomposition
  eig_sym_ordered(Sval,Svec,S);

  // Count number of eigenvalues that are above cutoff
  size_t Nind=0;
  for(size_t i=0;i<Nbf;i++)
    if(Sval(i)>=cutoff)
      Nind++;
  // Number of linearly dependent basis functions
  size_t Ndep=Nbf-Nind;

  // Form Shalf and Sinvhalf
  if(canonical) {
    // Canonical matrices - asymmetric
    Shalf.zeros(Nbf,Nind);
    Sinvh.zeros(Nbf,Nind);
    for(size_t i=0;i<Nind;i++) {
      size_t idx=Ndep+i;
      double ss=sqrt(Sval(idx));
      Shalf.col(i)=Svec.col(idx)*ss;
      Sinvh.col(i)=Svec.col(idx)/ss;
    }
  } else {   
    // Symmetric matrices
    Shalf.zeros(Nbf,Nbf);
    Sinvh.zeros(Nbf,Nbf);
    for(size_t i=0;i<Nind;i++) {
      size_t icol=Ndep+i;
      Sinvh+=Svec.col(icol)*arma::trans(Svec.col(icol))/sqrt(Sval(icol));
      Shalf+=Svec.col(icol)*arma::trans(Svec.col(icol))*sqrt(Sval(icol));
    }
  }
}


arma::vec MatToVec(const arma::mat & m) {
  // Size of vector to return
  size_t N=m.n_cols*m.n_rows;

  // Returned vector
  arma::vec ret(N);

  // Store matrix
  for(size_t i=0;i<m.n_rows;i++)
    for(size_t j=0;j<m.n_cols;j++)
      ret(i*m.n_cols+j)=m(i,j);

  return ret;
}

arma::mat VecToMat(const arma::vec & v, size_t nrows, size_t ncols) {

  // Check size consistency
  if(nrows*ncols!=v.n_elem) {
    ERROR_INFO();
    throw std::runtime_error("Cannot reshape a vector to a differently sized matrix.\n");
  }

  arma::mat m(nrows,ncols);
  m.zeros();

  for(size_t i=0;i<nrows;i++)
    for(size_t j=0;j<ncols;j++)
      m(i,j)=v(i*ncols+j);

  return m;
}

/// Get vector from cube: c(i,j,:)
arma::vec slicevec(const arma::cube & c, size_t i, size_t j) {
  arma::vec v(c.n_slices);
  for(size_t k=0;k<c.n_slices;k++)
    v(k)=c(i,j,k);
  return v;
}


arma::mat cos(const arma::mat & U) {
  // Compute eigendecomposition
  arma::vec evals;
  arma::mat evec;
  eig_sym_ordered(evals,evec,U);

  arma::mat cosU=U;

  // Check eigenvalues
  bool ok=0;
  for(size_t i=0;i<evals.n_elem;i++)
    if(fabs(evals(i))>DBL_EPSILON) {
      ok=1;
      break;
    }

  if(ok) {
    // OK to use general formula
    cosU.zeros();
    for(size_t i=0;i<evals.n_elem;i++)
      cosU+=std::cos(evals(i))*evec.col(i)*arma::trans(evec.col(i));
  } else {
    printf("Looks like U is singular. Using power expansion for cos.\n");

    // Zeroth order
    cosU.eye();
    // U^2
    arma::mat Usq=U*U;

    cosU+=0.5*Usq*(-1.0 + Usq*(1/12.0 - 1/360.0*Usq));
  }

  return cosU;
}

arma::mat sin(const arma::mat & U) {
  // Compute eigendecomposition
  arma::vec evals;
  arma::mat evec;
  eig_sym_ordered(evals,evec,U);

  arma::mat sinU=U;

  // Check eigenvalues
  bool ok=0;
  for(size_t i=0;i<evals.n_elem;i++)
    if(fabs(evals(i))>DBL_EPSILON) {
      ok=1;
      break;
    }

  if(ok) {
    // OK to use general formula
    sinU.zeros();
    for(size_t i=0;i<evals.n_elem;i++)
      sinU+=std::sin(evals(i))*evec.col(i)*arma::trans(evec.col(i));
  } else {
    printf("Looks like U is singular. Using power expansion for sin.\n");

    // U^2
    arma::mat Usq=U*U;

    sinU=U;
    sinU+=1.0/6.0*U*Usq*(-1.0 + 1.0/20.0*Usq*(1.0 - 1.0/42.0*Usq));
  }

  return sinU;
}

arma::mat sinc(const arma::mat & U) {
  // Compute eigendecomposition
  arma::vec evals;
  arma::mat evec;
  eig_sym_ordered(evals,evec,U);

  arma::mat sincU=U;

  // Check eigenvalues
  bool ok=0;
  for(size_t i=0;i<evals.n_elem;i++)
    if(fabs(evals(i))>DBL_EPSILON) {
      ok=1;
      break;
    }

  if(ok) {
    // OK to use general formula
    sincU.zeros();
    for(size_t i=0;i<evals.n_elem;i++)
      sincU+=sinc(evals(i))*evec.col(i)*arma::trans(evec.col(i));
  } else {
    printf("Looks like U is singular. Using power expansion for sinc.\n");

    // U^2
    arma::mat Usq=U*U;

    sincU.eye();
    sincU+=1.0/6.0*Usq*(-1.0 + 1.0/20.0*Usq*(1.0 - 1.0/42.0*Usq));
  }

  return sincU;
}

arma::mat sqrt(const arma::mat & M) {
  arma::vec evals;
  arma::mat evec;
  eig_sym_ordered(evals,evec,M);

  arma::mat sqrtM=M;

  // Check eigenvalues
  if(evals(0)<0) {
    ERROR_INFO();
    throw std::runtime_error("Negative eigenvalue of matrix!\n");
  }

  sqrtM.zeros();
  for(size_t i=0;i<evals.n_elem;i++)
    sqrtM+=std::sqrt(evals(i))*evec.col(i)*arma::trans(evec.col(i));

  return sqrtM;
}

arma::mat orthogonalize(const arma::mat & M) {
  // Decomposition: M = U s V'
  arma::mat U;
  arma::vec s;
  arma::mat V;
  bool svdok=arma::svd(U,s,V,M);
  if(!svdok) {
    ERROR_INFO();
    M.print("M");
    throw std::runtime_error("SVD failed.\n");
  }

  // Return matrix with singular values set to unity
  return U*arma::trans(V);
}

arma::cx_mat unitarize(const arma::cx_mat & M) {
  // Decomposition: M = U s V'
  arma::cx_mat U;
  arma::vec s;
  arma::cx_mat V;
  bool svdok=arma::svd(U,s,V,M);
  if(!svdok) {
    ERROR_INFO();
    M.print("M");
    throw std::runtime_error("SVD failed.\n");
  }

  // Return matrix with singular values set to unity
  return U*arma::trans(V);
}

arma::mat orthonormalize(const arma::mat & S, const arma::mat & C) {
  // Compute MO overlap
  arma::mat MOovl=arma::trans(C)*S*C;

  // Perform eigendecomposition
  arma::vec oval;
  arma::mat ovec;
  eig_sym_ordered(oval,ovec,MOovl);

  // New orbitals
  arma::mat Cnew(C*ovec);
  for(size_t io=0;io<C.n_cols;io++) {
    Cnew.col(io)/=sqrt(oval(io));
  }

  return Cnew;
}

arma::mat incomplete_cholesky(const arma::mat & M, size_t n) {
  if(M.n_rows != M.n_cols) {
    ERROR_INFO();
    throw std::runtime_error("Can't do Cholesky decomposition of non-square matrix!\n");
  }
  // Residual matrix
  arma::mat R(M);

  // Check sanity of argument
  if(n>M.n_rows)
    n=M.n_rows;

  // Returned matrix
  arma::mat C(R.n_rows,n);

  // Perform factorization
  size_t N=M.n_cols;
  for(size_t ii=0;ii<n;ii++) {
    // Find maximum diagonal element of residual
    size_t maxind=-1;
    double maxval=0.0;
    for(size_t j=0;j<N;j++)
      if(R(j,j)>maxval) {
        maxval=R(j,j);
        maxind=j;
      }
    // Take square root
    maxval=sqrt(maxval);

    // Added vector is
    C.col(ii)=R.col(maxind)/maxval;

    // Update remainder
    R-=C.col(ii)*arma::trans(C.col(ii));
  }

  return C;
}

void form_NOs(const arma::mat & P, const arma::mat & S, arma::mat & AO_to_NO, arma::mat & NO_to_AO, arma::vec & occs) {
  // Get canonical half-overlap and half-inverse overlap matrices
  arma::mat Sh, Sinvh;
  S_half_invhalf(S,Sh,Sinvh,true);

  // P in orthonormal basis is
  arma::mat P_orth=arma::trans(Sh)*P*Sh;

  // Diagonalize P to get NOs in orthonormal basis.
  arma::vec Pval;
  arma::mat Pvec;
  eig_sym_ordered(Pval,Pvec,P_orth);

  // Reverse ordering to get decreasing eigenvalues
  occs.zeros(Pval.n_elem);
  arma::mat Pv(Pvec.n_rows,Pvec.n_cols);
  for(size_t i=0;i<Pval.n_elem;i++) {
    size_t idx=Pval.n_elem-1-i;
    occs(i)=Pval(idx);
    Pv.col(i)=Pvec.col(idx);
  }

  /* Get NOs in AO basis. The natural orbital is written in the
     orthonormal basis as

     |i> = x_ai |a> = x_ai s_ja |j>
     = s_ja x_ai |j>
  */

  // The matrix that takes us from AO to NO is
  AO_to_NO=Sinvh*Pv;
  // and the one that takes us from NO to AO is
  NO_to_AO=arma::trans(Sh*Pv);
}


void form_NOs(const arma::mat & P, const arma::mat & S, arma::mat & AO_to_NO, arma::vec & occs) {
  arma::mat tmp;
  form_NOs(P,S,AO_to_NO,tmp,occs);
}

