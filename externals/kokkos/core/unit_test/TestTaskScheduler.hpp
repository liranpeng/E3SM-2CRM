/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Christian R. Trott (crtrott@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#ifndef KOKKOS_UNITTEST_TASKSCHEDULER_HPP
#define KOKKOS_UNITTEST_TASKSCHEDULER_HPP

#include <Kokkos_Macros.hpp>
#if defined( KOKKOS_ENABLE_TASKDAG )
#include <Kokkos_Core.hpp>
#include <cstdio>
#include <iostream>
#include <cmath>


namespace TestTaskScheduler {

namespace {

inline
long eval_fib( long n )
{
  constexpr long mask = 0x03;

  long fib[4] = { 0, 1, 1, 2 };

  for ( long i = 2; i <= n; ++i ) {
    fib[ i & mask ] = fib[ ( i - 1 ) & mask ] + fib[ ( i - 2 ) & mask ];
  }

  return fib[ n & mask ];
}

}

template< typename Scheduler >
struct TestFib
{
  using sched_type = Scheduler;
  using future_type = Kokkos::BasicFuture< long, Scheduler >;
  using value_type = long;

  future_type fib_m1;
  future_type fib_m2;
  const value_type n;

  KOKKOS_INLINE_FUNCTION
  TestFib( const value_type arg_n )
    : fib_m1(), fib_m2(), n( arg_n ) {}

  KOKKOS_INLINE_FUNCTION
  void operator()( typename sched_type::member_type & member, value_type & result )
  {
#if 0
    printf( "\nTestFib(%ld) %d %d\n", n, int( !fib_m1.is_null() ), int( !fib_m2.is_null() ) );
#endif

    auto& sched = member.scheduler();

    if ( n < 2 ) {
      result = n;
    }
    else if ( !fib_m2.is_null() && !fib_m1.is_null() ) {
      result = fib_m1.get() + fib_m2.get();
    }
    else {
      // Spawn new children and respawn myself to sum their results.
      // Spawn lower value at higher priority as it has a shorter
      // path to completion.

      fib_m2 = Kokkos::task_spawn( Kokkos::TaskSingle( sched, Kokkos::TaskPriority::High )
                                 , TestFib( n - 2 ) );

      fib_m1 = Kokkos::task_spawn( Kokkos::TaskSingle( sched )
                                 , TestFib( n - 1 ) );

      Kokkos::BasicFuture<void, Scheduler> dep[] = { fib_m1, fib_m2 };
      Kokkos::BasicFuture<void, Scheduler> fib_all = sched.when_all( dep, 2 );

      if ( !fib_m2.is_null() && !fib_m1.is_null() && !fib_all.is_null() ) {
        // High priority to retire this branch.
        Kokkos::respawn( this, fib_all, Kokkos::TaskPriority::High );
      }
      else {
#if 1
        printf( "TestFib(%ld) insufficient memory alloc_capacity(%d) task_max(%d) task_accum(%ld)\n"
               , n
               , 0 //sched.allocation_capacity()
               , 0 //sched.allocated_task_count_max()
               , 0l //sched.allocated_task_count_accum()
               );
#endif

        Kokkos::abort( "TestFib insufficient memory" );

      }
    }
  }

  static void run( int i, size_t MemoryCapacity = 16000 )
  {
    typedef typename sched_type::memory_space memory_space;

    enum { MinBlockSize   =   64 };
    enum { MaxBlockSize   = 1024 };
    enum { SuperBlockSize = 4096 };

    sched_type root_sched( memory_space()
                         , MemoryCapacity
                         , MinBlockSize
                         , std::min(size_t(MaxBlockSize),MemoryCapacity)
                         , std::min(size_t(SuperBlockSize),MemoryCapacity) );

    future_type f = Kokkos::host_spawn( Kokkos::TaskSingle( root_sched )
                                      , TestFib( i ) );

    Kokkos::wait( root_sched );

    ASSERT_EQ( eval_fib( i ), f.get() );

#if 0
    fprintf( stdout, "\nTestFib::run(%d) spawn_size(%d) when_all_size(%d) alloc_capacity(%d) task_max(%d) task_accum(%ld)\n"
           , i
           , int(root_sched.template spawn_allocation_size<TestFib>())
           , int(root_sched.when_all_allocation_size(2))
           , root_sched.allocation_capacity()
           , root_sched.allocated_task_count_max()
           , root_sched.allocated_task_count_accum()
           );
    fflush( stdout );
#endif
  }
};

} // namespace TestTaskScheduler

//----------------------------------------------------------------------------

namespace TestTaskScheduler {

template< class Scheduler >
struct TestTaskDependence {
  typedef Scheduler  sched_type;
  typedef Kokkos::BasicFuture< void, Scheduler > future_type;
  typedef Kokkos::View< long, typename sched_type::execution_space >     accum_type;
  typedef void                            value_type;

  accum_type  m_accum;
  long        m_count;

  KOKKOS_INLINE_FUNCTION
  TestTaskDependence( long n
                    , const accum_type & arg_accum )
    : m_accum( arg_accum )
    , m_count( n ) {}

  KOKKOS_INLINE_FUNCTION
  void operator()( typename sched_type::member_type & member )
  {
    auto& sched = member.scheduler();
    enum { CHUNK = 8 };
    const int n = CHUNK < m_count ? CHUNK : m_count;

    if ( 1 < m_count ) {

      const int increment = ( m_count + n - 1 ) / n;

      future_type f =
        sched.when_all( n , [this,&member,increment]( int i ) {
          const long inc   = increment ;
          const long begin = i * inc ;
          const long count = begin + inc < m_count ? inc : m_count - begin ;

          return Kokkos::task_spawn
            ( Kokkos::TaskSingle( member.scheduler() )
            , TestTaskDependence( count, m_accum ) );
        });

      m_count = 0;

      Kokkos::respawn( this, f );
    }
    else if ( 1 == m_count ) {
      Kokkos::atomic_increment( & m_accum() );
    }
  }

  static void run( int n )
  {
    typedef typename sched_type::memory_space memory_space;

    enum { MemoryCapacity = 16000 };
    enum { MinBlockSize   =   64 };
    enum { MaxBlockSize   = 1024 };
    enum { SuperBlockSize = 4096 };

    sched_type sched( memory_space()
                    , MemoryCapacity
                    , MinBlockSize
                    , MaxBlockSize
                    , SuperBlockSize );

    accum_type accum( "accum" );

    typename accum_type::HostMirror host_accum = Kokkos::create_mirror_view( accum );

    Kokkos::host_spawn( Kokkos::TaskSingle( sched ), TestTaskDependence( n, accum ) );

    Kokkos::wait( sched );

    Kokkos::deep_copy( host_accum, accum );

    ASSERT_EQ( host_accum(), n );
  }
};

} // namespace TestTaskScheduler

//----------------------------------------------------------------------------

namespace TestTaskScheduler {

template< class Scheduler >
struct TestTaskTeam {
  //enum { SPAN = 8 };
  enum { SPAN = 33 };
  //enum { SPAN = 1 };

  typedef void                                value_type;
  using sched_type = Scheduler;
  using future_type = Kokkos::BasicFuture<void, sched_type>;
  using ExecSpace = typename sched_type::execution_space;
  typedef Kokkos::View< long*, ExecSpace >    view_type;

  future_type  future;

  view_type   parfor_result;
  view_type   parreduce_check;
  view_type   parscan_result;
  view_type   parscan_check;
  const long  nvalue;

  KOKKOS_INLINE_FUNCTION
  TestTaskTeam( const view_type  & arg_parfor_result
              , const view_type  & arg_parreduce_check
              , const view_type  & arg_parscan_result
              , const view_type  & arg_parscan_check
              , const long         arg_nvalue )
    : future()
    , parfor_result( arg_parfor_result )
    , parreduce_check( arg_parreduce_check )
    , parscan_result( arg_parscan_result )
    , parscan_check( arg_parscan_check )
    , nvalue( arg_nvalue ) {}

  KOKKOS_INLINE_FUNCTION
  void operator()( typename sched_type::member_type & member )
  {
    auto& sched = member.scheduler();
    const long end   = nvalue + 1;
    // begin = max(end - SPAN, 0);
    const long begin = 0 < end - SPAN ? end - SPAN : 0;

    if ( 0 < begin && future.is_null() ) {
      if ( member.team_rank() == 0 ) {
        future = Kokkos::task_spawn( Kokkos::TaskTeam( sched )
                                   , TestTaskTeam( parfor_result
                                                 , parreduce_check
                                                 , parscan_result
                                                 , parscan_check
                                                 , begin - 1 )
                                   );

        #if !defined(__HCC_ACCELERATOR__) && !defined(__CUDA_ARCH__)
        assert( !future.is_null() );
        #endif

        Kokkos::respawn( this, future );
      }

      return;
    }

    Kokkos::parallel_for( Kokkos::TeamThreadRange( member, begin, end )
                        , [&] ( int i ) { parfor_result[i] = i; }
                        );

    // Test parallel_reduce without join.

    long tot = 0;
    long expected = ( begin + end - 1 ) * ( end - begin ) * 0.5;

    Kokkos::parallel_reduce( Kokkos::TeamThreadRange( member, begin, end )
                           , [&] ( int i, long & res ) { res += parfor_result[i]; }
                           , tot
                           );

    Kokkos::parallel_for( Kokkos::TeamThreadRange( member, begin, end )
                        , [&] ( int i ) { parreduce_check[i] = expected - tot; }
                        );

    // Test parallel_reduce with join.

    tot = 0;
    Kokkos::parallel_reduce( Kokkos::TeamThreadRange( member, begin, end )
                           , [&] ( int i, long & res ) { res += parfor_result[i]; }
                           , Kokkos::Sum<long>( tot )
                           );

    Kokkos::parallel_for( Kokkos::TeamThreadRange( member, begin, end )
                        , [&] ( int i ) { parreduce_check[i] += expected - tot; }
                        );

    // Test parallel_scan.

    // Exclusive scan.
    Kokkos::parallel_scan<long>( Kokkos::TeamThreadRange( member, begin, end )
                               , [&] ( int i, long & val, const bool final )
    {
      if ( final ) { parscan_result[i] = val; }

      val += i;
    });

    // Wait for 'parscan_result' before testing it.
    member.team_barrier();

    if ( member.team_rank() == 0 ) {
      for ( long i = begin; i < end; ++i ) {
        parscan_check[i] = ( i * ( i - 1 ) - begin * ( begin - 1 ) ) * 0.5 - parscan_result[i];
      }
    }

    // Don't overwrite 'parscan_result' until it has been tested.
    member.team_barrier();

    // Inclusive scan.
    Kokkos::parallel_scan<long>( Kokkos::TeamThreadRange( member, begin, end )
                               , [&] ( int i, long & val, const bool final )
    {
      val += i;

      if ( final ) { parscan_result[i] = val; }
    });

    // Wait for 'parscan_result' before testing it.
    member.team_barrier();

    if ( member.team_rank() == 0 ) {
      for ( long i = begin; i < end; ++i ) {
        parscan_check[i] += ( i * ( i + 1 ) - begin * ( begin - 1 ) ) * 0.5 - parscan_result[i];
      }
    }

    // ThreadVectorRange check.
/*
    long result = 0;
    expected = ( begin + end - 1 ) * ( end - begin ) * 0.5;
    Kokkos::parallel_reduce( Kokkos::TeamThreadRange( member, 0, 1 )
                           , [&] ( const int i, long & outerUpdate )
    {
      long sum_j = 0.0;

      Kokkos::parallel_reduce( Kokkos::ThreadVectorRange( member, end - begin )
                             , [&] ( const int j, long & innerUpdate )
      {
        innerUpdate += begin + j;
      }, sum_j );

      outerUpdate += sum_j;
    }, result );

    Kokkos::parallel_for( Kokkos::TeamThreadRange( member, begin, end )
                        , [&] ( int i )
    {
      parreduce_check[i] += result - expected;
    });
*/

  }

  static void run( long n )
  {
    const unsigned memory_capacity = 400000;

    enum { MinBlockSize   =   64 };
    enum { MaxBlockSize   = 1024 };
    enum { SuperBlockSize = 4096 };

    sched_type root_sched( typename sched_type::memory_space()
                         , memory_capacity
                         , MinBlockSize
                         , MaxBlockSize
                         , SuperBlockSize );

    view_type root_parfor_result( "parfor_result", n + 1 );
    view_type root_parreduce_check( "parreduce_check", n + 1 );
    view_type root_parscan_result( "parscan_result", n + 1 );
    view_type root_parscan_check( "parscan_check", n + 1 );

    typename view_type::HostMirror
      host_parfor_result = Kokkos::create_mirror_view( root_parfor_result );
    typename view_type::HostMirror
      host_parreduce_check = Kokkos::create_mirror_view( root_parreduce_check );
    typename view_type::HostMirror
      host_parscan_result = Kokkos::create_mirror_view( root_parscan_result );
    typename view_type::HostMirror
      host_parscan_check = Kokkos::create_mirror_view( root_parscan_check );

    future_type f = Kokkos::host_spawn( Kokkos::TaskTeam( root_sched )
                                      , TestTaskTeam( root_parfor_result
                                                    , root_parreduce_check
                                                    , root_parscan_result
                                                    , root_parscan_check
                                                    , n )
                                      );

    Kokkos::wait( root_sched );

    Kokkos::deep_copy( host_parfor_result, root_parfor_result );
    Kokkos::deep_copy( host_parreduce_check, root_parreduce_check );
    Kokkos::deep_copy( host_parscan_result, root_parscan_result );
    Kokkos::deep_copy( host_parscan_check, root_parscan_check );

    long error_count = 0 ;

    for ( long i = 0; i <= n; ++i ) {
      const long answer = i;

      if ( host_parfor_result( i ) != answer ) {
        ++error_count ;
        std::cerr << "TestTaskTeam::run ERROR parallel_for result(" << i << ") = "
                  << host_parfor_result( i ) << " != " << answer << std::endl;
      }

      if ( host_parreduce_check( i ) != 0 ) {
        ++error_count ;
        std::cerr << "TestTaskTeam::run ERROR parallel_reduce check(" << i << ") = "
                  << host_parreduce_check( i ) << " != 0" << std::endl;
      }

      if ( host_parscan_check( i ) != 0 ) {
        ++error_count ;
        std::cerr << "TestTaskTeam::run ERROR parallel_scan check(" << i << ") = "
                  << host_parscan_check( i ) << " != 0" << std::endl;
      }
    }

    ASSERT_EQ( 0L , error_count );
  }
};

template< class Scheduler >
struct TestTaskTeamValue {
  enum { SPAN = 8 };

  typedef long                                     value_type;
  using sched_type = Scheduler;
  using future_type = Kokkos::BasicFuture< value_type, sched_type >;
  using ExecSpace = typename sched_type::execution_space;
  typedef Kokkos::View< long*, ExecSpace >         view_type;

  future_type  future;

  view_type   result;
  const long  nvalue;

  KOKKOS_INLINE_FUNCTION
  TestTaskTeamValue( const view_type  & arg_result
                   , const long         arg_nvalue )
    : future()
    , result( arg_result )
    , nvalue( arg_nvalue ) {}

  KOKKOS_INLINE_FUNCTION
  void operator()( typename sched_type::member_type const & member
                 , value_type & final )
  {
    const long end   = nvalue + 1;
    const long begin = 0 < end - SPAN ? end - SPAN : 0;

    auto& sched = member.scheduler();

    if ( 0 < begin && future.is_null() ) {
      if ( member.team_rank() == 0 ) {
        future = sched.task_spawn( TestTaskTeamValue( result, begin - 1 )
                                 , Kokkos::TaskTeam );

        #if !defined(__HCC_ACCELERATOR__) && !defined(__CUDA_ARCH__)
        assert( !future.is_null() );
        #endif

        sched.respawn( this , future );
      }

      return;
    }

    Kokkos::parallel_for( Kokkos::TeamThreadRange( member, begin, end )
                        , [&] ( int i ) { result[i] = i + 1; }
                        );

    if ( member.team_rank() == 0 ) {
      final = result[nvalue];
    }

    Kokkos::memory_fence();
  }

  static void run( long n )
  {
    const unsigned memory_capacity = 100000;

    enum { MinBlockSize   =   64 };
    enum { MaxBlockSize   = 1024 };
    enum { SuperBlockSize = 4096 };

    sched_type root_sched( typename sched_type::memory_space()
                         , memory_capacity
                         , MinBlockSize
                         , MaxBlockSize
                         , SuperBlockSize );

    view_type root_result( "result", n + 1 );

    typename view_type::HostMirror host_result = Kokkos::create_mirror_view( root_result );

    future_type fv = root_sched.host_spawn( TestTaskTeamValue( root_result, n )
                                          , Kokkos::TaskTeam );

    Kokkos::wait( root_sched );

    Kokkos::deep_copy( host_result, root_result );

    if ( fv.get() != n + 1 ) {
      std::cerr << "TestTaskTeamValue ERROR future = "
                << fv.get() << " != " << n + 1 << std::endl;
    }

    for ( long i = 0; i <= n; ++i ) {
      const long answer = i + 1;

      if ( host_result( i ) != answer ) {
        std::cerr << "TestTaskTeamValue ERROR result(" << i << ") = "
                  << host_result( i ) << " != " << answer << std::endl;
      }
    }
  }
};

} // namespace TestTaskScheduler

//----------------------------------------------------------------------------

namespace TestTaskScheduler {

template< class Scheduler >
struct TestTaskSpawnWithPool {
  using sched_type = Scheduler;
  using future_type = Kokkos::BasicFuture<void, sched_type>;
  typedef void                            value_type;
  using Space = typename sched_type::execution_space;

  int  m_count ;
  Kokkos::MemoryPool<Space> m_pool ;

  KOKKOS_INLINE_FUNCTION
  TestTaskSpawnWithPool(
    const int & arg_count,
    const Kokkos::MemoryPool<Space> & arg_pool
  )
    : m_count( arg_count )
    , m_pool( arg_pool )
    {}

  KOKKOS_INLINE_FUNCTION
  void operator()( typename sched_type::member_type & member )
  {
    if ( m_count ) {
      Kokkos::task_spawn( Kokkos::TaskSingle( member.scheduler() ) , TestTaskSpawnWithPool( m_count - 1, m_pool ) );
    }
  }

  static void run()
  {
    typedef typename sched_type::memory_space memory_space;

    enum { MemoryCapacity = 16000 };
    enum { MinBlockSize   =   64 };
    enum { MaxBlockSize   = 1024 };
    enum { SuperBlockSize = 4096 };

    sched_type sched( memory_space()
                    , MemoryCapacity
                    , MinBlockSize
                    , MaxBlockSize
                    , SuperBlockSize );

    using other_memory_space = typename Space::memory_space;
    Kokkos::MemoryPool<Space> pool(other_memory_space(), 10000, 100, 200, 1000);
    auto f = Kokkos::host_spawn( Kokkos::TaskSingle( sched ), TestTaskSpawnWithPool( 3, pool ) );

    Kokkos::wait( sched );
  }
};

}

//----------------------------------------------------------------------------


namespace TestTaskScheduler {

template<class Scheduler>
struct TestMultipleDependence {

  using sched_type = Scheduler;
  using future_bool = Kokkos::BasicFuture<bool, sched_type>;
  using future_int = Kokkos::BasicFuture<int, sched_type>;
  using value_type = bool;
  using execution_space = typename sched_type::execution_space;

  enum : int { NPerDepth = 6 };
  enum : int { NFanout = 3 };

  // xlC doesn't like incomplete aggregate constructors, so we have do do this manually:
  TestMultipleDependence(int depth, int max_depth)
    : m_depth(depth),
      m_max_depth(max_depth),
      m_dep()
  { 
    // gcc 4.8 has an internal compile error when I give the initializer in the class, so I have do do it here
    for(int i = 0; i < NPerDepth; ++i) {
      m_result_futures[i] = future_bool();
    }
  }

  // xlC doesn't like incomplete aggregate constructors, so we have do do this manually:
  TestMultipleDependence(int depth, int max_depth, future_int dep)
    : m_depth(depth),
      m_max_depth(max_depth),
      m_dep(dep)
  { 
    // gcc 4.8 has an internal compile error when I give the initializer in the class, so I have do do it here
    for(int i = 0; i < NPerDepth; ++i) {
      m_result_futures[i] = future_bool();
    }
  }

  int m_depth;
  int m_max_depth;
  future_int m_dep;
  future_bool m_result_futures[NPerDepth];


  struct TestCheckReady {
     future_int m_dep;
     using value_type = bool;
     KOKKOS_INLINE_FUNCTION
     void operator()(typename Scheduler::member_type&, bool& value) {
       // if it was "transiently" ready, this could be false even if we made it a dependence of this task
       value = m_dep.is_ready();
       return;
     }
  };
     

  struct TestComputeValue {
    using value_type = int;
    KOKKOS_INLINE_FUNCTION
    void operator()(typename Scheduler::member_type&, int& result) {
      double value = 0;
      // keep this one busy for a while
      for(int i = 0; i < 10000; ++i) {
        value += i * i / 7.138 / value;
      }
      // Do something irrelevant
      result = int(value) << 2;
      return;
    }
  };


  KOKKOS_INLINE_FUNCTION
  void operator()(typename sched_type::member_type & member, bool& value)
  {
    if(m_result_futures[0].is_null()) {
      if (m_depth == 0) {
        // Spawn one expensive task at the root
        m_dep = Kokkos::task_spawn(Kokkos::TaskSingle(member.scheduler()), TestComputeValue{});
      }

      // Then check for it to be ready in a whole bunch of other tasks that race
      int n_checkers = NPerDepth;
      if(m_depth < m_max_depth) {
        n_checkers -= NFanout;
        for(int i = n_checkers; i < NPerDepth; ++i) {
          m_result_futures[i] = Kokkos::task_spawn(Kokkos::TaskSingle(member.scheduler()),
            TestMultipleDependence<Scheduler>(m_depth + 1, m_max_depth, m_dep)
          );
        }
      }

      for(int i = 0; i < n_checkers; ++i) {
        m_result_futures[i] = member.scheduler().spawn(Kokkos::TaskSingle(m_dep), TestCheckReady{m_dep});
      }
      auto done = member.scheduler().when_all(m_result_futures, NPerDepth);
      Kokkos::respawn(this, done);

      return;
    }
    else {
      value = true;
      for(int i = 0; i < NPerDepth; ++i) {
        value = value && !m_result_futures[i].is_null();
        if(value) {
          value = value && m_result_futures[i].get();
        }
      }
      return;
    }
  }

  static void run(int depth)
  {
    typedef typename sched_type::memory_space memory_space;

    enum { MemoryCapacity = 1 << 30 };
    enum { MinBlockSize   =   64 };
    enum { MaxBlockSize   = 1024 };
    enum { SuperBlockSize = 4096 };

    sched_type sched( memory_space()
                    , MemoryCapacity
                    , MinBlockSize
                    , MaxBlockSize
                    , SuperBlockSize );

    auto f = Kokkos::host_spawn( Kokkos::TaskSingle( sched ), TestMultipleDependence<Scheduler>( 0, depth )  );

    Kokkos::wait( sched );

    ASSERT_TRUE( f.get() );

  }
};

}

namespace Test {

TEST_F( TEST_CATEGORY, task_fib )
{
  const int N = 27 ;
  for ( int i = 0; i < N; ++i ) {
    TestTaskScheduler::TestFib< Kokkos::DeprecatedTaskScheduler<TEST_EXECSPACE> >::run( i , ( i + 1 ) * ( i + 1 ) * 2000 );
  }
}

TEST_F( TEST_CATEGORY, task_fib_multiple )
{
  const int N = 27 ;
  for ( int i = 0; i < N; ++i ) {
    TestTaskScheduler::TestFib< Kokkos::DeprecatedTaskSchedulerMultiple<TEST_EXECSPACE> >::run( i , ( i + 1 ) * ( i + 1 ) * 8000 );
  }
}

TEST_F( TEST_CATEGORY, task_fib_new )
{
  const int N = 27 ;
  for ( int i = 0; i < N; ++i ) {
    TestTaskScheduler::TestFib< Kokkos::TaskScheduler<TEST_EXECSPACE> >::run( i , ( i + 1 ) * ( i + 1 ) * 2000 );
  }
}

TEST_F( TEST_CATEGORY, task_fib_new_multiple )
{
  const int N = 27 ;
  for ( int i = 0; i < N; ++i ) {
    TestTaskScheduler::TestFib< Kokkos::TaskSchedulerMultiple<TEST_EXECSPACE> >::run( i , ( i + 1 ) * ( i + 1 ) * 64000 );
  }
}

TEST_F( TEST_CATEGORY, task_fib_chase_lev )
{
  const int N = 27 ;
  for ( int i = 0; i < N; ++i ) {
    TestTaskScheduler::TestFib< Kokkos::ChaseLevTaskScheduler<TEST_EXECSPACE> >::run( i , ( i + 1 ) * ( i + 1 ) * 64000 );
  }
}

TEST_F( TEST_CATEGORY, task_depend )
{
  for ( int i = 0; i < 25; ++i ) {
    TestTaskScheduler::TestTaskDependence< Kokkos::DeprecatedTaskScheduler<TEST_EXECSPACE> >::run( i );
  }
}

TEST_F( TEST_CATEGORY, task_depend_multiple )
{
  for ( int i = 0; i < 25; ++i ) {
    TestTaskScheduler::TestTaskDependence< Kokkos::DeprecatedTaskSchedulerMultiple<TEST_EXECSPACE> >::run( i );
  }
}

TEST_F( TEST_CATEGORY, task_depend_new )
{
  for ( int i = 0; i < 25; ++i ) {
    TestTaskScheduler::TestTaskDependence< Kokkos::TaskScheduler<TEST_EXECSPACE> >::run( i );
  }
}

TEST_F( TEST_CATEGORY, task_depend_new_multiple )
{
  for ( int i = 0; i < 25; ++i ) {
    TestTaskScheduler::TestTaskDependence< Kokkos::TaskSchedulerMultiple<TEST_EXECSPACE> >::run( i );
  }
}

TEST_F( TEST_CATEGORY, task_depend_chase_lev )
{
  for ( int i = 0; i < 25; ++i ) {
    TestTaskScheduler::TestTaskDependence< Kokkos::ChaseLevTaskScheduler<TEST_EXECSPACE> >::run( i );
  }
}

TEST_F( TEST_CATEGORY, task_team )
{
  TestTaskScheduler::TestTaskTeam< Kokkos::DeprecatedTaskScheduler<TEST_EXECSPACE> >::run( 1000 );
  //TestTaskScheduler::TestTaskTeamValue< TEST_EXECSPACE >::run( 1000 ); // Put back after testing.
}

TEST_F( TEST_CATEGORY, task_team_multiple )
{
  TestTaskScheduler::TestTaskTeam< Kokkos::DeprecatedTaskSchedulerMultiple<TEST_EXECSPACE> >::run( 1000 );
  //TestTaskScheduler::TestTaskTeamValue< TEST_EXECSPACE >::run( 1000 ); // Put back after testing.
}

TEST_F( TEST_CATEGORY, task_team_new )
{
  TestTaskScheduler::TestTaskTeam< Kokkos::TaskScheduler<TEST_EXECSPACE> >::run( 1000 );
}

TEST_F( TEST_CATEGORY, task_team_new_multiple )
{
  TestTaskScheduler::TestTaskTeam< Kokkos::TaskSchedulerMultiple<TEST_EXECSPACE> >::run( 1000 );
}

TEST_F( TEST_CATEGORY, task_with_mempool )
{
  TestTaskScheduler::TestTaskSpawnWithPool< Kokkos::DeprecatedTaskScheduler<TEST_EXECSPACE> >::run();
}

TEST_F( TEST_CATEGORY, task_with_mempool_multiple )
{
  TestTaskScheduler::TestTaskSpawnWithPool< Kokkos::DeprecatedTaskSchedulerMultiple<TEST_EXECSPACE> >::run();
}

TEST_F( TEST_CATEGORY, task_with_mempool_new )
{
  TestTaskScheduler::TestTaskSpawnWithPool< Kokkos::TaskScheduler<TEST_EXECSPACE> >::run();
}

TEST_F( TEST_CATEGORY, task_with_mempool_new_multiple )
{
  TestTaskScheduler::TestTaskSpawnWithPool< Kokkos::TaskSchedulerMultiple<TEST_EXECSPACE> >::run();
}

TEST_F( TEST_CATEGORY, task_multiple_depend )
{
  for ( int i = 2; i < 6; ++i ) {
    TestTaskScheduler::TestMultipleDependence< Kokkos::DeprecatedTaskScheduler<TEST_EXECSPACE> >::run( i );
  }
}

TEST_F( TEST_CATEGORY, task_multiple_depend_new )
{
  for ( int i = 2; i < 6; ++i ) {
    TestTaskScheduler::TestMultipleDependence< Kokkos::TaskScheduler<TEST_EXECSPACE> >::run( i );
  }
}

TEST_F( TEST_CATEGORY, task_multiple_depend_new_multiple )
{
  for ( int i = 2; i < 6; ++i ) {
    TestTaskScheduler::TestMultipleDependence< Kokkos::TaskSchedulerMultiple<TEST_EXECSPACE> >::run( i );
  }
}

TEST_F( TEST_CATEGORY, task_multiple_depend_chases_lev )
{
  for ( int i = 2; i < 6; ++i ) {
    TestTaskScheduler::TestMultipleDependence< Kokkos::ChaseLevTaskScheduler<TEST_EXECSPACE> >::run( i );
  }
}

}

#endif // #if defined( KOKKOS_ENABLE_TASKDAG )
#endif // #ifndef KOKKOS_UNITTEST_TASKSCHEDULER_HPP

