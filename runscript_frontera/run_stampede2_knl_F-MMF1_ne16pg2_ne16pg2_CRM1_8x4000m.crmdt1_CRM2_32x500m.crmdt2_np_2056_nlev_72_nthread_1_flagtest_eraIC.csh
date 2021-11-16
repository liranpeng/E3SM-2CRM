#!/bin/bash
for COUNTER in 2008 #2009 2010 2011 2012 2013 2014 2015 2016 2017 2018
do
sed -e "s/ERAYYY/${COUNTER}/g;" stampede2_knl_F-MMF1_ne16pg2_ne16pg2_CRM1_64x1000m.crmdt1_CRM2_512x250m.crmdt2_np_2056_nlev_72_nthread_1_flagtest.csh > stampede2_knl_F-MMF1_ne16pg2_ne16pg2_CRM1_64x1000m.crmdt1_CRM2_512x250m.crmdt2_np_2056_nlev_72_nthread_1_flagtest_eraIC_${COUNTER}.csh

chmod 700 stampede2_knl_F-MMF1_ne16pg2_ne16pg2_CRM1_64x1000m.crmdt1_CRM2_512x250m.crmdt2_np_2056_nlev_72_nthread_1_flagtest_eraIC_${COUNTER}.csh
./stampede2_knl_F-MMF1_ne16pg2_ne16pg2_CRM1_64x1000m.crmdt1_CRM2_512x250m.crmdt2_np_2056_nlev_72_nthread_1_flagtest_eraIC_${COUNTER}.csh

done

#mv science_ne16_test_08102020_* /home1/07088/tg863871/temp/
