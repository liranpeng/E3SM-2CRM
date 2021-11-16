! Set the domain dimensionality, size and number of subdomains.

module domain
  use advection, only: NADV, NADVS

  use crmdims
  implicit none

  integer :: YES3D = -1  ! Domain dimensionality: 1 - 3D, 0 - 2D
  integer :: nx_gl = -1 ! Number of grid points in X
  integer :: ny_gl = -1 ! Number of grid points in Y
  integer :: nz_gl = -1 ! Number of pressure (scalar) levels

  real(crm_rknd) :: dx = -1.  ! grid spacing in x direction
  real(crm_rknd) :: dy = -1.   ! grid spacing in y direction
  
  integer, parameter :: nsubdomains_x  = 1 ! No of subdomains in x
  integer, parameter :: nsubdomains_y  = 1 ! No of subdomains in y


  ! define # of points in x and y direction to average for
  !   output relating to statistical moments.
  ! For example, navgmom_x = 8 means the output will be an 8 times coarser grid than the original.
  ! If don't wanna such output, just set them to -1 in both directions.
  ! See Changes_log/README.UUmods for more details.
  integer, parameter :: navgmom_x = -1
  integer, parameter :: navgmom_y = -1

  integer, parameter :: ntracers = 0 ! number of transported tracers (dotracers=.true.)

  ! Note:
  !  * nx_gl and ny_gl should be a factor of 2,3, or 5 (see User's Guide)
  !  * if 2D case, ny_gl = nsubdomains_y = 1 ;
  !  * nsubdomains_x*nsubdomains_y = total number of processors
  !  * if one processor is used, than  nsubdomains_x = nsubdomains_y = 1;
  !  * if ntracers is > 0, don't forget to set dotracers to .true. in namelist

contains

  subroutine setup_domain_xy(dx_gl_in, dy_gl_in)
    real(crm_rknd), intent(in) :: dx_gl_in, dy_gl_in
    real(crm_rknd) :: dx,dy
       dx = dx_gl_in
       dy = dy_gl_in
  end subroutine setup_domain_xy
  
end module domain
