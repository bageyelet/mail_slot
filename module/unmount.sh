sudo rmmod mail_spot_proc_metadata
sudo rmmod mail_spot

if [ -e "/dev/ms0" ]; then
    sudo rm /dev/ms0
fi

if [ -e "/dev/ms1" ]; then
    sudo rm /dev/ms1
fi