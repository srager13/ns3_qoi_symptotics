#!/bin/bash

# top-k unicast files
scp ./src/applications/model/topk* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/model/
scp ./src/applications/helper/topk* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/helper/

# qoi flood files
scp ./src/applications/model/qoi* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/model/
scp ./src/applications/helper/qoi* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/helper/

# TDMA files
scp -r ./src/simple-wireless-tdma/model/* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/simple-wireless-tdma/model/
scp -r ./src/simple-wireless-tdma/helper/* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/simple-wireless-tdma/helper/

scp ./scratch/topk* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/scratch/
scp ./scratch/qoi* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/scratch/

scp ./run_topk_query*.sh str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/
scp ./run_qoi_query*.sh str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/

