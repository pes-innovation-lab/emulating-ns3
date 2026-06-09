sudo ifconfig br-left down
sudo ifconfig br-right down
sudo ip link set tap-left nomaster
sudo ip link set tap-right nomaster
sudo ip link del br-left
sudo ip link del br-right
sudo ifconfig tap-left down
sudo ifconfig tap-right down
sudo ip link del tap-left
sudo ip link del tap-right
