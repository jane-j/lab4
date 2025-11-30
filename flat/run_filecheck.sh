cd apps/file_check
make clean
cd ../../os
make clean
cd ..
mainframer.sh 'cd os && make'
mainframer.sh 'cd apps/file_check && make'
cd apps/file_check
make run > out.txt

make clean
cd ../../os
make clean
cd ..
