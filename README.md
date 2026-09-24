# UDP Puzzle Solver
This program takes in an IP address and four puzzle ports.  
It finds which puzzle belongs to which port and then solves each puzzle.

## Development Environment
The program was developed and tested on Ubuntu through WSL on Windows.

## Compile program
Compile the program by running:

```bash
make
```

## Find Open Ports
Before running the program, find the four open UDP ports: 

```bash
sudo nmap -sU -p 4000-4100 130.208.246.98
```

## Run Program
Run the program using:
```bash
sudo ./puzzlesolver <IPaddress> <port1> <port2> <port3> <port4>
```

For example:
```bash
sudo ./puzzlesolver 130.208.246.98 4024 4032 4043 4061
```
The program needs to be run with `sudo` because the Evil puzzle uses a raw socket.

## Known Issues
UDP packets may occasionally be lost so the program may need to be run again if a puzzle fails.