#ifndef __TACHO_SYMMETRIZE_INTERNAL_HPP__
#define __TACHO_SYMMETRIZE_INTERNAL_HPP__

/// \file  Tacho_Symmetrize_Internal.hpp
/// \brief Symmetrize a square block matrix
/// \author Kyungjoo Kim (kyukim@sandia.gov)

namespace Tacho {

template <> struct Symmetrize<Uplo::Upper, Algo::Internal> {
  template <typename MemberType, typename ViewTypeA>
  KOKKOS_INLINE_FUNCTION static int invoke(MemberType &member, const ViewTypeA &A) {
    const ordinal_type m = A.extent(0), n = A.extent(1);

    if (m == n) {
      if (A.span() > 0) {
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
        Kokkos::parallel_for(Kokkos::TeamThreadRange(member, n), [&](const ordinal_type &j) {
          Kokkos::parallel_for(Kokkos::ThreadVectorRange(member, j), [&](const ordinal_type &i) { A(j, i) = A(i, j); });
        });
#else
        for (ordinal_type j = 0; j < n; ++j)
          for (ordinal_type i = 0; i < j; ++i)
            A(j, i) = A(i, j);
#endif
      }
    } else {
      printf("Error: Symmetrize<Algo::Internal> A is not square\n");
    }
    return 0;
  }
};

template <> struct Symmetrize<Uplo::Lower, Algo::Internal> {
  template <typename MemberType, typename ViewTypeA>
  KOKKOS_INLINE_FUNCTION static int invoke(MemberType &member, const ViewTypeA &A) {
    const ordinal_type m = A.extent(0), n = A.extent(1);

    if (m == n) {
      if (A.span() > 0) {
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
        Kokkos::parallel_for(Kokkos::TeamThreadRange(member, n), [&](const ordinal_type &j) {
          Kokkos::parallel_for(Kokkos::ThreadVectorRange(member, j), [&](const ordinal_type &i) { A(i, j) = A(j, i); });
        });
#else
        for (ordinal_type j = 0; j < n; ++j)
          for (ordinal_type i = 0; i < j; ++i)
            A(i, j) = A(j, i);
#endif
      }
    } else {
      printf("Error: Symmetrize<Algo::Internal> A is not square\n");
    }
    return 0;
  }
};

} // namespace Tacho
#endif
