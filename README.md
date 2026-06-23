# wire
extremely simple messaging protocol
## usage
### server
1. get https://github.com/uncognic/cast
2. `cast build --release`
3. set motd in .wire/motd if you want
4. `./build/release/wire -p 7070`

### client
1. `./build/release/wirec <server_ip> <server_port>`
2. type username and press enter
3. `/join #<room_name>` if you are the first user you will be op
4. message!
