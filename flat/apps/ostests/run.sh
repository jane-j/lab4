make clean
cd ../../os
make clean
cd ..
mainframer.sh 'cd os && make'
mainframer.sh 'cd apps/ostests && make'
cd apps/ostests
make run > out.txt
