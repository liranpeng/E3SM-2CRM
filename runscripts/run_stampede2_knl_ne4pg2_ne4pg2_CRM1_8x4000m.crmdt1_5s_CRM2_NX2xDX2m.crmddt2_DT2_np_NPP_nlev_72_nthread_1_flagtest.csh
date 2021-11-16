#!/bin/bash
for COUNTER in 130 132 136
do
for COUNTER1 in 0.625 1.25 2.5
do
for COUNTER2 in 512 1024 2048
do
sed -e "s/NPP/${COUNTER}/g;s/DT2/${COUNTER1}/g;s/NX2/${COUNTER2}/g;" stampede2_knl_ne4pg2_ne4pg2_CRM1_8x4000m.crmdt1_5s_CRM2_NX2xDX2m.crmddt2_DT2_np_NPP_nlev_72_nthread_1_flagtest.csh > stampede2_knl_ne4pg2_ne4pg2_CRM1_8x4000m.crmdt1_5s_CRM2_${COUNTER2}xDX2m.crmddt2_${COUNTER1}_np_${COUNTER}_nlev_72_nthread_1_flagtest.csh

chmod 700 stampede2_knl_ne4pg2_ne4pg2_CRM1_8x4000m.crmdt1_5s_CRM2_${COUNTER2}xDX2m.crmddt2_${COUNTER1}_np_${COUNTER}_nlev_72_nthread_1_flagtest.csh
./stampede2_knl_ne4pg2_ne4pg2_CRM1_8x4000m.crmdt1_5s_CRM2_${COUNTER2}xDX2m.crmddt2_${COUNTER1}_np_${COUNTER}_nlev_72_nthread_1_flagtest.csh

done
done
done

#mv science_ne16_test_08102020_* /home1/07088/tg863871/temp/
