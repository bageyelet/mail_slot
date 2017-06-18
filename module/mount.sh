cd ./actual_ms
make
cd ../proc_metadata
make 
cd ..

sudo insmod ./actual_ms/mail_spot.ko
sudo insmod ./proc_metadata/mail_spot_proc_metadata.ko

if [ -f "/dev/ms0" ]; then
    sudo chmod 666 /dev/ms0
else
    sudo mknod /dev/ms0 c 250 0
    sudo chmod 666 /dev/ms0
fi

if [ -f "/dev/ms1" ]; then
    sudo chmod 666 /dev/ms1
else
    sudo mknod /dev/ms1 c 250 1
    sudo chmod 666 /dev/ms1
fi
