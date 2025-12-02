cd apps/pattern
make clean
cd ../../os
make clean
cd ..
mainframer.sh 'cd os && make'
mainframer.sh 'cd apps/pattern && make'
cd apps/pattern
make run > out.txt

make clean
cd ../../os
make clean
cd ..
