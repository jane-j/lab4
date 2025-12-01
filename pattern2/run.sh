rm /tmp/niharika.img
cd apps/fdisk
make clean
cd ../../os
make clean
cd ..
mainframer.sh 'cd os && make'
mainframer.sh 'cd apps/fdisk && make'
cd apps/fdisk
make run > out.txt

make clean
cd ../../os
make clean
cd ..
