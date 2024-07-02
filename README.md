# README

## Introduction
This program allows for the execution of a given command or copies stdin to stdout if no command is provided. It also supports TCP, UDP, and UDS (Unix Domain Socket) server/client communication.

## Usage

### Question 1
Run the program by typing:
./ttt 123456789

- The alien will be the first to play.
- If it prints "I win", the alien won.
- If it prints "I lose", the human won.

### Question 2
Run the program by typing:
./mync -e "./ttt 123456789"

- The program will execute the `ttt` program and play the game with the alien.

### Question 3
Several options to run the program:

1. Run the program and get input from port 4050:
./mync -e "./ttt 123456789" -i TCPS4050

   - On another terminal, run the client with:
./mync -o TCPClocalhost,4050


2. Run the program and send output to port 4050:
./mync -e "./ttt 123456789" -o TCPS4050

   - On another terminal, run the client with:
./mync -i TCPClocalhost,4050


3. Run the program and get input and send output on port 4050:
./mync -e "./ttt 123456789" -b TCPS4050

   - On another terminal, use:
nc localhost 4050


4. Run the program, get input from port 4050, and send output to port 4455:
./mync -e "./ttt 123456789" -i TCPS4050 -o TCPC4455

   - Steps:
     1. `make; clear; ./mync -i TCPS4050`
     2. `clear; ./mync -e "./ttt 123456789" -i TCPS4455 -o TCPClocalhost,4050`
     3. `clear; ./mync -o TCPClocalhost,4455`

### Question 3.5
If there is no `-e "./ttt 123456789"` in the command, the program acts as a simple netcat program.
   - To run:
     ```
     make; clear; ./mync -i TCPS4050
     ```
     - On another terminal:
     ```
     clear; ./mync -o TCPClocalhost,4050
     ```

### Question 4
1. Run the program and get input from port 4050 with a timeout of 10 seconds:
./mync -e "./ttt 123456789" -i UDPS4050 -t 10

   - On another terminal:
./mync -o UDPClocalhost,4050 -t 10

2. Run the program, get input from port 4050, and send output to port 4455:
- Steps:
  1. `make; clear; ./mync -i TCPS4050`
  2. `clear; ./mync -e "./ttt 123456789" -i UDPS4455 -o TCPClocalhost,4050`
  3. `clear; ./mync -o UDPClocalhost,4455`

### Question 6: UDS (Unix Domain Sockets)
#### Stream
- Stream server:
  ```
  ./mync -e "./ttt 123456789" -i UDSSS/tmp/my_stream_socket
  ./mync -e "./ttt 123456789" -o UDSSS/tmp/my_stream_socket
  ./mync -e "./ttt 123456789" -b UDSSS/tmp/my_stream_socket
  ```
- Stream client:
  ```
  ./mync -e "./ttt 123456789" -i UDSCS/tmp/my_stream_socket
  ./mync -e "./ttt 123456789" -o UDSCS/tmp/my_stream_socket
  ./mync -e "./ttt 123456789" -b UDSCS/tmp/my_stream_socket
  ```

#### Datagram
- Datagram server (only -i is possible):
  ```
  ./mync -e "./ttt 123456789" -i UDSSD/tmp/my_datagram_socket
  ```
- Datagram client (only -o is possible):
  ```
  ./mync -e "./ttt 123456789" -o UDSCD/tmp/my_datagram_socket
  ```

## Testing
### Case 1:
1. On terminal 1:
make; clear; ./mync -e "./ttt 123456789" -i UDSSS/tmp/my_stream_socket

2. On terminal 2:
clear; ./mync -o UDSCS/tmp/my_stream_socket


## Additional Notes
- `./mync` supports both TCP and UDP protocols for server-client communication.
- For TCP/UDP, specify server with `TCPS` or `UDPS` and client with `TCPC` or `UDPC`.
- For UDS, specify stream with `UDSSS` (server) and `UDSCS` (client), and datagram with `UDSSD` (server) and `UDSCD` (client).
- Use `-i` for input, `-o` for output, and `-b` for both input and output.
- When no `-e` option is given, `./mync` acts like a simple netcat program, copying stdin to stdout.

