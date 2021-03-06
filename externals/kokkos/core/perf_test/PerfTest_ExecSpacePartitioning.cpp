#include <Kokkos_Core.hpp>
#include <gtest/gtest.h>
#include <PerfTest_Category.hpp>


namespace Test {

namespace {
  template<class ExecSpace>
  struct SpaceInstance {
    static ExecSpace create() {
      return ExecSpace();
    }
    static void destroy(ExecSpace&) {
    }
    static bool overlap() {
      return false;
    }
  };

  #ifdef KOKKOS_ENABLE_CUDA
  template<>
  struct SpaceInstance<Kokkos::Cuda> {
    static Kokkos::Cuda create() {
      cudaStream_t stream;
      cudaStreamCreate(&stream);
      return Kokkos::Cuda(stream);
    }
    static void destroy(Kokkos::Cuda& space) {
      cudaStream_t stream = space.cuda_stream();
      cudaStreamDestroy(stream);
    }
    static bool overlap() {
      bool value = true;
      auto local_rank_str = std::getenv("CUDA_LAUNCH_BLOCKING");
      if(local_rank_str) {
        value = (std::atoi(local_rank_str)==0);
      }
      return value;
    }
  };
  #endif
}

struct FunctorRange {
  int M,R;
  Kokkos::View<double**,TEST_EXECSPACE> a;
  FunctorRange(int M_, int R_, Kokkos::View<double**,TEST_EXECSPACE> a_):M(M_),R(R_),a(a_){}
  KOKKOS_INLINE_FUNCTION
  void operator() (const int i) const {
    for(int r=0;r<R;r++)
    for(int j=0;j<M;j++) {
      a(i,j)+=1.0;
    }
  }
};

struct FunctorMDRange {
  int M,R;
  Kokkos::View<double**,TEST_EXECSPACE> a;
  FunctorMDRange(int M_, int R_, Kokkos::View<double**,TEST_EXECSPACE> a_):M(M_),R(R_),a(a_){}
  KOKKOS_INLINE_FUNCTION
  void operator() (const int i, const int) const {
    for(int j=0;j<M;j++)
      a(i,j)+=1.0;
  }
};

struct FunctorTeam {
  int M,R;
  Kokkos::View<double**,Kokkos::LayoutRight,TEST_EXECSPACE> a;
  FunctorTeam(int M_, int R_, Kokkos::View<double**,Kokkos::LayoutRight,TEST_EXECSPACE> a_):M(M_),R(R_),a(a_){}
  KOKKOS_INLINE_FUNCTION
  void operator() (const Kokkos::TeamPolicy<TEST_EXECSPACE>::member_type& team) const {
    int i = team.league_rank();
    for(int r=0;r<R;r++) {
      Kokkos::parallel_for(Kokkos::TeamThreadRange(team,M), [&] (const int j) {
        a(i,j)+=1.0;
      });
    }
  }
};


TEST_F( default_exec, overlap_range_policy ) {
  int N = 2000;
   int M = 10000;
   int R =  10;

   TEST_EXECSPACE space;
   TEST_EXECSPACE space1 = SpaceInstance<TEST_EXECSPACE>::create();
   TEST_EXECSPACE space2 = SpaceInstance<TEST_EXECSPACE>::create();

   Kokkos::View<double**,TEST_EXECSPACE> a("A",N,M);
   FunctorRange f(M,R,a);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel0",
       Kokkos::RangePolicy<TEST_EXECSPACE>(0,N), FunctorRange(M,R,a));

   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel1",
       Kokkos::Experimental::require(
           Kokkos::RangePolicy<TEST_EXECSPACE>(space1,0,N),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel2",
       Kokkos::Experimental::require(
           Kokkos::RangePolicy<TEST_EXECSPACE>(space2,0,N),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::fence();

   Kokkos::Timer timer;
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel3",
       Kokkos::Experimental::require(
           Kokkos::RangePolicy<TEST_EXECSPACE>(space,0,N),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel4",
       Kokkos::Experimental::require(
           Kokkos::RangePolicy<TEST_EXECSPACE>(space,0,N),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::fence();

   timer.reset();
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel5",
       Kokkos::Experimental::require(
           Kokkos::RangePolicy<TEST_EXECSPACE>(space1,0,N),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , FunctorRange(M,R,a));
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel6",
       Kokkos::Experimental::require(
           Kokkos::RangePolicy<TEST_EXECSPACE>(space2,0,N),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , FunctorRange(M,R,a));
   Kokkos::fence();
   double time_overlap = timer.seconds();

   timer.reset();
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel7",
       Kokkos::Experimental::require(
           Kokkos::RangePolicy<TEST_EXECSPACE>(space,0,N),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel8",
       Kokkos::Experimental::require(
           Kokkos::RangePolicy<TEST_EXECSPACE>(space,0,N),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::fence();
   double time_end = timer.seconds();

   SpaceInstance<TEST_EXECSPACE>::destroy(space1);
   SpaceInstance<TEST_EXECSPACE>::destroy(space2);

   if(SpaceInstance<TEST_EXECSPACE>::overlap()) {
     ASSERT_TRUE( (time_end > 1.5*time_overlap) );
   }
   printf("Time RangePolicy: NonOverlap: %lf Time Overlap: %lf\n",time_end,time_overlap);
}

TEST_F( default_exec, overlap_mdrange_policy ) {
  int N = 200;
   int M = 10000;
   int R =  10;

   TEST_EXECSPACE space;
   TEST_EXECSPACE space1 = SpaceInstance<TEST_EXECSPACE>::create();
   TEST_EXECSPACE space2 = SpaceInstance<TEST_EXECSPACE>::create();

   Kokkos::View<double**,TEST_EXECSPACE> a("A",N,M);
   FunctorMDRange f(M,R,a);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel0",
       Kokkos::Experimental::require(
           Kokkos::MDRangePolicy<TEST_EXECSPACE,Kokkos::Rank<2>>({0,0},{N,R}),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , FunctorMDRange(M,R,a));

   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel1",
       Kokkos::Experimental::require(
           Kokkos::MDRangePolicy<TEST_EXECSPACE,Kokkos::Rank<2>>(space1,{0,0},{N,R}),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel2",
       Kokkos::Experimental::require(
           Kokkos::MDRangePolicy<TEST_EXECSPACE,Kokkos::Rank<2>>(space2,{0,0},{N,R}),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::fence();

   Kokkos::Timer timer;
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel3",
       Kokkos::Experimental::require(
           Kokkos::MDRangePolicy<TEST_EXECSPACE,Kokkos::Rank<2>>(space,{0,0},{N,R}),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel4",
       Kokkos::Experimental::require(
           Kokkos::MDRangePolicy<TEST_EXECSPACE,Kokkos::Rank<2>>(space,{0,0},{N,R}),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::fence();

   timer.reset();
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel5",
       Kokkos::Experimental::require(
           Kokkos::MDRangePolicy<TEST_EXECSPACE,Kokkos::Rank<2>>(space1,{0,0},{N,R}),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , FunctorMDRange(M,R,a));
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel6",
       Kokkos::Experimental::require(
           Kokkos::MDRangePolicy<TEST_EXECSPACE,Kokkos::Rank<2>>(space2,{0,0},{N,R}),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , FunctorMDRange(M,R,a));
   Kokkos::fence();
   double time_overlap = timer.seconds();

   timer.reset();
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel7",
       Kokkos::Experimental::require(
           Kokkos::MDRangePolicy<TEST_EXECSPACE,Kokkos::Rank<2>>(space,{0,0},{N,R}),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel8",
       Kokkos::Experimental::require(
           Kokkos::MDRangePolicy<TEST_EXECSPACE,Kokkos::Rank<2>>(space,{0,0},{N,R}),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::fence();
   double time_end = timer.seconds();

   SpaceInstance<TEST_EXECSPACE>::destroy(space1);
   SpaceInstance<TEST_EXECSPACE>::destroy(space2);

   if(SpaceInstance<TEST_EXECSPACE>::overlap()) {
     ASSERT_TRUE( (time_end > 1.5*time_overlap) );
   }
   printf("Time MDRangePolicy: NonOverlap: %lf Time Overlap: %lf\n",time_end,time_overlap);
}

TEST_F( default_exec, overlap_team_policy ) {
  int N = 20;
   int M = 1000000;
   int R =  10;

   TEST_EXECSPACE space;
   TEST_EXECSPACE space1 = SpaceInstance<TEST_EXECSPACE>::create();
   TEST_EXECSPACE space2 = SpaceInstance<TEST_EXECSPACE>::create();

   Kokkos::View<double**,Kokkos::LayoutRight,TEST_EXECSPACE> a("A",N,M);
   FunctorTeam f(M,R,a);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel0",
       Kokkos::Experimental::require(
           Kokkos::TeamPolicy<TEST_EXECSPACE>(N,Kokkos::AUTO),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , FunctorTeam(M,R,a));

   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel1",
       Kokkos::Experimental::require(
           Kokkos::TeamPolicy<TEST_EXECSPACE>(space1,N,Kokkos::AUTO),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel2",
       Kokkos::Experimental::require(
           Kokkos::TeamPolicy<TEST_EXECSPACE>(space2,N,Kokkos::AUTO),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::fence();

   Kokkos::Timer timer;
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel3",
       Kokkos::Experimental::require(
           Kokkos::TeamPolicy<TEST_EXECSPACE>(space,N,Kokkos::AUTO),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel4",
       Kokkos::Experimental::require(
           Kokkos::TeamPolicy<TEST_EXECSPACE>(space,N,Kokkos::AUTO),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::fence();

   timer.reset();
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel5",
       Kokkos::Experimental::require(
           Kokkos::TeamPolicy<TEST_EXECSPACE>(space1,N,Kokkos::AUTO),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , FunctorTeam(M,R,a));
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel6",
       Kokkos::Experimental::require(
           Kokkos::TeamPolicy<TEST_EXECSPACE>(space2,N,Kokkos::AUTO),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , FunctorTeam(M,R,a));
   Kokkos::fence();
   double time_overlap = timer.seconds();

   timer.reset();
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel7",
       Kokkos::Experimental::require(
           Kokkos::TeamPolicy<TEST_EXECSPACE>(space,N,Kokkos::AUTO),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::parallel_for("default_exec::overlap_range_policy::kernel8",
       Kokkos::Experimental::require(
           Kokkos::TeamPolicy<TEST_EXECSPACE>(space,N,Kokkos::AUTO),
           Kokkos::Experimental::WorkItemProperty::HintLightWeight)
       , f);
   Kokkos::fence();
   double time_end = timer.seconds();

   SpaceInstance<TEST_EXECSPACE>::destroy(space1);
   SpaceInstance<TEST_EXECSPACE>::destroy(space2);

   if(SpaceInstance<TEST_EXECSPACE>::overlap()) {
     ASSERT_TRUE( (time_end > 1.5*time_overlap) );
   }
   printf("Time TeamPolicy: NonOverlap: %lf Time Overlap: %lf\n",time_end,time_overlap);
}
}
