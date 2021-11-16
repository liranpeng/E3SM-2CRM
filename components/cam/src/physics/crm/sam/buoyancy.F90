
module buoyancy_mod
  implicit none

contains

  subroutine buoyancy(ncrms)
    use vars
    use params
    implicit none
    integer, intent(in) :: ncrms
    integer i,j,k,kb,icrm
    real du(ncrms,nx,ny,nz,3)
    real(crm_rknd) betu, betd

#if defined(_OPENACC)
    !$acc parallel loop gang vector collapse(4) async(asyncid)
#elif defined(_OPENMP)
    !$omp target teams distribute parallel do collapse(4)
#endif

  do k=1,nzm
    do j=1,ny
      do i=1,nx
        do icrm=1,ncrms
         du(icrm,i,j,k,3)=dwdt(icrm,i,j,k,na)
       end do
      end do
    end do
  end do

    do k=2,nzm
      do j=1,ny
        do i=1,nx
          do icrm=1,ncrms
            kb=k-1
            betu=adz(icrm,kb)/(adz(icrm,k)+adz(icrm,kb))
            betd=adz(icrm,k)/(adz(icrm,k)+adz(icrm,kb))
#if defined(_OPENMP)
            !$omp atomic update
#endif
            dwdt(icrm,i,j,k,na)=dwdt(icrm,i,j,k,na) +  &
            bet(icrm,k)*betu* &
            ( tabs0(icrm,k)*(epsv*(qv(icrm,i,j,k)-qv0(icrm,k))-(qcl(icrm,i,j,k)+qci(icrm,i,j,k)-qn0(icrm,k)+qpl(icrm,i,j,k)+qpi(icrm,i,j,k)-qp0(icrm,k))) &
            +(tabs(icrm,i,j,k)-tabs0(icrm,k))*(1.+epsv*qv0(icrm,k)-qn0(icrm,k)-qp0(icrm,k)) ) &
            + bet(icrm,kb)*betd* &
            ( tabs0(icrm,kb)*(epsv*(qv(icrm,i,j,kb)-qv0(icrm,kb))-(qcl(icrm,i,j,kb)+qci(icrm,i,j,kb)-qn0(icrm,kb)+qpl(icrm,i,j,kb)+qpi(icrm,i,j,kb)-qp0(icrm,kb))) &
            +(tabs(icrm,i,j,kb)-tabs0(icrm,kb))*(1.+epsv*qv0(icrm,kb)-qn0(icrm,kb)-qp0(icrm,kb)) )
          end do ! i
        end do ! j
      end do ! k
    end do ! k

!to calculate the buoyancy profile.
  do k=1,nzm
    do j=1,ny
      do i=1,nx
        do icrm=1,ncrms
          du(icrm,i,j,k,1)=0.
          du(icrm,i,j,k,2)=0.
          du(icrm,i,j,k,3)=dwdt(icrm,i,j,k,na)-du(icrm,i,j,k,3)
        end do
      end do
    end do
  end do

  call stat_tke(du,tkelebuoy,ncrms)


  end subroutine buoyancy
end module buoyancy_mod

! TKE budget stuff
! copied in from SAM 6.10.6 code to allow computation of addtional statistics
! mwyant 3/1/2016

! hparish tests this routine and confirms functionality after few modifications. 04/01/2016

subroutine stat_tke(du,tkele,ncrms)

use vars
use cam_logfile,     only: iulog
implicit none
real du(ncrms,nx,ny,nz,3)
real tkele(ncrms,nzm)
real d_u(ncrms,nz), d_v(ncrms,nz),d_w(ncrms,nz),coef
integer, intent(in) :: ncrms
integer i,j,k,icrm
coef = 1./float(nx*ny)
do k=1,nz
  do icrm=1,ncrms
   d_u(icrm,k)=0.
   d_v(icrm,k)=0.
   d_w(icrm,k)=0.
  end do
end do
do k=1,nzm
 do j=1,ny
  do i=1,nx
    do icrm=1,ncrms
     d_u(icrm,k)=d_u(icrm,k)+(u(icrm,i,j,k)-u0(icrm,k))*du(icrm,i,j,k,1)
     d_v(icrm,k)=d_v(icrm,k)+(v(icrm,i,j,k)-v0(icrm,k))*du(icrm,i,j,k,2)
     d_w(icrm,k)=d_w(icrm,k)+ w(icrm,i,j,k) *      du(icrm,i,j,k,3)
    end do
  end do
 end do
 d_u(icrm,k)=d_u(icrm,k)*coef
 d_v(icrm,k)=d_v(icrm,k)*coef
 d_w(icrm,k)=d_w(icrm,k)*coef
end do
do k=1,nzm
  do icrm=1,ncrms
   tkele(icrm,k)=0.5*(d_w(icrm,k)+d_w(icrm,k+1))+d_u(icrm,k)+d_v(icrm,k)*YES3D
  end do
end do

end
