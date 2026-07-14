We create an evaluation bench to identify the standard deviations of a given protocol implementation in ns-3 that conforms to the real world implementation. The bench is of 2 parts:-

### First part: 
2 containers where 2 applications communicate with each other, and logs metrics, according to the preferred testing applications for each protocol:-
- ccperf and iperf3 for TCP, UDP
- perfdhcp for DHCP
- arping for ARP

### Second Part: 

2 nodes inside ns-3 that simulate the exact same exchanges by the 2 applications inside the simulation. The same metrics as the first part are logged.

From the collected logs, we come up with standard deviations for which it is an acceptable simulation. The final standard deviations are then noted down in a table. This standard deviation is used to score the protocol implementation out of 10.

### Scoring:

This scoring of the implementation should be modifiable according to the user when they perform their own testing.

This scoring is then used to score implementations like in refactor/base, where the testing happens between ns-3 node and the real application containers.

## Format:

A `config.toml` file that contains:-
- protocol
- scoring for std deviation
- container (path/oci)
- timeout
- number of runs
- log_file_location
- corresponding ns-3 script to run

A python script that:-
- reads the conig.toml
- spins up the applications
- runs ns-3 simulation
- extracts performance metrics and establish baseline
- compare and get score
- give output and diff.