#ifndef __OS_TESTS__
#define __OS_TESTS__

#define NUMBYTES 1024
#define NUMBYTES_2 2048
#define LARGE_NUM_BLOCKS 400

void RunOSTests();
void TestBasic();
void TestUnaligned();
void TestSparse();
void TestLargeFile();
void TestDeleteAndReopen();
void TestPersistence();

#endif
