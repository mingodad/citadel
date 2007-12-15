#!/bin/bash
cp ~vs/citadelPrepare/home/checkout/tmp/webcit_*   ~vs/apache/home/debiancitadel/public_html/source/
cp ~vs/citadelPrepare/home/checkout/tmp/citadel_*  ~vs/apache/home/debiancitadel/public_html/source/

chroot ~vs/apache  /bin/bash -c "cd /home/debiancitadel/; ./refreshsource.sh"
