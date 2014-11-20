#!/bin/bash

scp ./src/applications/model/topk* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/model/
scp ./src/applications/helper/topk* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/helper/
scp ./src/applications/model/qoi* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/model/
scp ./src/applications/helper/qoi* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/src/applications/helper/

scp ./scratch/topk* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/scratch/
scp ./scratch/qoi* str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/scratch/

scp ./run_topk_query*.sh str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/
scp ./run_qoi_query*.sh str5004@village.cse.psu.edu:/home/moby/str5004/ns-3-working-directory/ns-allinone-3.21/ns-3.21/

