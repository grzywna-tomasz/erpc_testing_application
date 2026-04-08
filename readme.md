This application is used to test https://github.com/grzywna-tomasz/Nucleo_F439ZI project. It is using TCP/IP as a transport layer

# Create generator
Ideally follow the erpc repo readme. Here is quick recap:
## Things to be installed:
Currently the application is tested using wsl
- Cmake
- gcc
- g++ (sudo apt install g++)
- bison (sudo apt install bison)
- flex 
## Creating generator
- go to erpc
- run command "cmake -B ./build"
- enter build directory
- run command "make" - it will create everything including the generator (erpc/build/erpcgen/erpcgen)
## Generate files
interface.erpc is where the interfaces are defined.
"./erpc/build/erpcgen/erpcgen --output generated interface.erpc"

