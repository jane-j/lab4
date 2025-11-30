default:
	cd file_check; make

clean:
	cd file_check; make clean

run:
	cd ../../os; make; cd ../bin; dlxsim -x os.dlx.obj -a -D F -u file_check.dlx.obj; ee469_fixterminal
