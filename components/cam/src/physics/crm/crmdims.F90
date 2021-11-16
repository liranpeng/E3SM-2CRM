module crmdims

    use params, only: crm_rknd

    implicit none

    integer, parameter ::  crm_nx=CRM_NX
    integer, parameter ::  crm_ny=CRM_NY
    integer, parameter ::  crm_nz=CRM_NZ

    integer, parameter ::  crm_nx_rad=CRM_NX_RAD
    integer, parameter ::  crm_ny_rad=CRM_NY_RAD
    
    integer, parameter ::  crm_nx2=CRM_NX2
    integer, parameter ::  crm_ny2=CRM_NY2
    integer, parameter ::  crm_nz2=CRM_NZ2

    integer, parameter ::  crm_nx_rad2=CRM_NX_RAD2
    integer, parameter ::  crm_ny_rad2=CRM_NY_RAD2

    real(crm_rknd), parameter :: crm_dx=CRM_DX
    real(crm_rknd), parameter :: crm_dy=crm_dx
    real(crm_rknd), parameter :: crm_dt=CRM_DT
    
    real(crm_rknd), parameter :: crm_dx2=CRM_DX2
    real(crm_rknd), parameter :: crm_dy2=crm_dx2
    real(crm_rknd), parameter :: crm_dt2=CRM_DT2 

end module crmdims
