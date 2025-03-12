
# The Deribit Order Management in C++ 
The Deribit Order Management in C++ is a trading system designed for seamless interaction with the Deribit API, enabling efficient order execution and real-time order book data streaming. This project provides a WebSocket-based order book and trading solution for Spot, Futures, and Options instruments, ensuring low-latency market access.



Interface
![Interface](https://github.com/Yxssh/goQuantTask/blob/307c3b09b9e8b494a913495cc56fe20a3808e1c3/screenshot/interface.png)


## Building Instructions

Before compiling and running this project, ensure you have the following dependencies installed and correctly configured.


## Deployment

1. 1. Install MSYS2 and MinGW-W64

```bash
  pacman -Syu  # Update system
  pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make

```
2. Required Libraries and Headers
  - cURL (curl.h)
  - JSON (nlohmann/json.hpp)
  - ASIO (asio/ssl.hpp)
  - (websocketpp/client.hpp)
  - OpenSSL (ssl.h)

3. Environment Configuration
  Ensure the MinGW64 bin directory is added to your PATH

4. Compilation Command
Navigate to the project directory and compile the code using g++ from MinGW64:
```bash
$ g++ -g -o goQuantTask goQuantTask.cpp -I. -Iinclude -I/mingw64/include -L/mingw64/lib -lssl -lcrypto -lcurl -lws2_32 -lpthread -lz

```
5. Running the Program
After successful compilation, run the executable:
```bash
./goQuantTask
```



## Core Functions
1. Place Order
2. Cancel Order
3.  Modify order
4. Get orderbook 
5. View current positions
6. Real-time market data streaming via WebSocket 
   - Implement WebSocket server functionality 
   - Allow clients to subscribe to symbols 
   - Stream continuous orderbook updates for subscribed symbols 



## Market Coverage
- Instruments: Spot, Futures, and Options 
- Scope: All supported symbols 