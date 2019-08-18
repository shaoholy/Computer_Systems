# runs all test script
chmod 777 test-1.sh
chmod 777 test-2.sh
chmod 777 test-fill_full_disk_test.sh
chmod 777 test-3.sh

{
./test-1.sh
sleep 2
./test-2.sh
sleep 2
./test-fill_full_disk_test.sh
sleep 2
./test-3.sh
}



