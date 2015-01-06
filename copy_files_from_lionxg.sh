#!/bin/bash

scp str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/model/topk* ./src/applications/model/
scp str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/helper/topk* ./src/applications/helper/

# TDMA files
scp -r str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/simple-wireless-tdma/model/* ./src/simple-wireless-tdma/model/

scp str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/scratch/topk* ./scratch/

scp str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/run_topk_query*.sh ./
