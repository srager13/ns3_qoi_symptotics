#!/bin/bash

# topk-query application files
scp ./src/applications/model/topk* str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/model/
scp ./src/applications/helper/topk* str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/helper/

# TDMA files
scp -r ./src/simple-wireless-tdma/model/* str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/simple-wireless-tdma/model/

# topk run scripts
scp ./scratch/topk* str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/scratch/
scp ./run_topk_query*.sh str5004@lionxg.rcc.psu.edu:/gpfs/home/str5004/work/ns-3-working-directory/ns-allinone-3.21/ns-3.21/

