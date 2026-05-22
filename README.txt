# SUB-MPSI

This project implements SUB-MPSI, an information-theoretically secure multi-party private set intersection protocol.

## Envirment

This code and following instructions is tested on Ubuntu 20.04, with g++ 11.5.0, Cmake 3.16.3, and GNU Make 4.2.1. To run this code, a CPU with SGX support is necessary (check with `grep -q sgx /proc/cpuinfo`). Alternatively, you can run this code in SGX simulation mode.

## Building the Project

### 1. Install Intel SGX

```bash
# Download the driver
wget https://download.01.org/intel-sgx/sgx-linux/2.13/distro/ubuntu18.04-server/sgx_linux_x64_driver_2.11.0_0373e2e.bin

# Install the driver
sudo chmod +x sgx_linux_x64_driver_2.11.0_0373e2e.bin
sudo ./sgx_linux_x64_driver_2.11.0_0373e2e.bin

# Install dependencies
sudo apt-get install build-essential ocaml ocamlbuild automake autoconf libtool wget python libssl-dev git cmake perl

# Clone and build SGX SDK
git clone https://github.com/intel/linux-sgx.git
cd linux-sgx
make preparation
make sdk
make sdk_install_pkg

# Install the SDK package
cd linux/installer/bin
sudo ./sgx_linux_x64_sdk_*.bin

# Install dependencies
sudo apt-get install build-essential ocaml ocamlbuild automake autoconf libtool wget python libssl-dev git cmake perl

# Clone and build SGX SDK
git clone https://github.com/intel/linux-sgx.git
cd linux-sgx
make preparation
make sdk
make sdk_install_pkg

# Install the SDK package
cd linux/installer/bin
sudo ./sgx_linux_x64_sdk_*.bin
# When prompted, install to /opt/intel/

# Add repository and install PSW
echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu bionic main' | sudo tee /etc/apt/sources.list.d/intel-sgx.list
wget -qO - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | sudo apt-key add -
sudo apt-get update
sudo apt-get install libsgx-launch libsgx-urts libsgx-epid libsgx-quote-ex libsgx-dcap-ql
```

### 2. Build coproto

```
git clone https://github.com/ladnir/coproto.git
cd coproto
python3 build.py
```

### 3. Compile MPSI

```bash
git clone https://anonymous.4open.science/r/SUB-MPSI-CCD8
cd SUB-MPSI-CCD8
source /opt/intel/sgxsdk/environment
make # Use "make SGX_MODE=SIM" for running in simulation mode
```

## Running the Code

##### Print help information

```bash
./main
```

##### example running of our protcols

Run our SPIR with a database size of $1024$, a domain size of $97$, and a batch size of $10$:

```bash
./app -running spir -m 1024 -q 97 -batch 10
```

Run our UZS with $5$ participants, each having a set size of $1024$ and a universal set size of $4096$:

```bash
./app -running uzs -n 5 -m 1024 -d 4096
```

Run our MPSI with $20$ participants, each having a set size of $1024$, a universal set size of $32768$, and an intersection size of $100$:

```
./app -running mpsi -n 20 -m 1024 -d 32768 -i 100
```

##### Parameters

- `-running`: The protocol to execute; supported values are "spir", "uzs", and "mpsi".
- `-n`: The number of participants in MPSI and UZS.
- `-m`: The database size in SPIR, or the individual set size per participant in MPSI and UZS.
- `-d`: The universal set size in MPSI and UZS.
- -i: The expected intersection size in MPSI.
- -q: The domain size of each element within the database in SPIR.
- -batch: The batch size in SPIR.