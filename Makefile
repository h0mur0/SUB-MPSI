######## SGX SDK Settings ########

SGX_SDK ?= /opt/intel/sgxsdk
SGX_MODE ?= HW
SGX_ARCH ?= x64
SGX_DEBUG ?= 1

include $(SGX_SDK)/buildenv.mk

# 检测架构
ifeq ($(shell getconf LONG_BIT), 32)
    SGX_ARCH := x86
else ifeq ($(findstring -m32, $(CXXFLAGS)), -m32)
    SGX_ARCH := x86
endif

ifeq ($(SGX_ARCH), x86)
    SGX_COMMON_FLAGS := -m32
    SGX_LIBRARY_PATH := $(SGX_SDK)/lib
    SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x86/sgx_sign
    SGX_EDGER8R := $(SGX_SDK)/bin/x86/sgx_edger8r
else
    SGX_COMMON_FLAGS := -m64
    SGX_LIBRARY_PATH := $(SGX_SDK)/lib64
    SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x64/sgx_sign
    SGX_EDGER8R := $(SGX_SDK)/bin/x64/sgx_edger8r
endif

# 编译标志设置
ifeq ($(SGX_DEBUG), 1)
    SGX_COMMON_FLAGS += -O0 -g
else
    SGX_COMMON_FLAGS += -O2
endif

SGX_COMMON_FLAGS += -Wall -Wextra -Winit-self -Wpointer-arith -Wreturn-type \
                    -Waddress -Wsequence-point -Wformat-security \
                    -Wmissing-include-dirs -Wfloat-equal -Wundef -Wshadow \
                    -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls
SGX_COMMON_CFLAGS := $(SGX_COMMON_FLAGS) -Wjump-misses-init -Wstrict-prototypes -Wunsuffixed-float-constants

# 【修改1】关键修改：将标准改为 C++20 以支持 coproto 和协程
SGX_COMMON_CXXFLAGS := $(SGX_COMMON_FLAGS) -Wnon-virtual-dtor -std=c++20

######## App Settings ########

ifneq ($(SGX_MODE), HW)
    Urts_Library_Name := sgx_urts_sim
else
    Urts_Library_Name := sgx_urts
endif

# 【修改2】关键修改：自动包含 App 目录下所有的协议源代码
App_Cpp_Files := $(wildcard App/*.cpp)
App_Include_Paths := -IInclude -IApp -I$(SGX_SDK)/include

App_C_Flags := -fPIC -Wno-attributes $(App_Include_Paths)

ifeq ($(SGX_DEBUG), 1)
    App_C_Flags += -DDEBUG -UNDEBUG -UEDEBUG
else
    App_C_Flags += -DNDEBUG -UEDEBUG -UDEBUG
endif

App_Cpp_Flags := $(App_C_Flags)

# 【修改3】关键修改：链接 coproto 库及其可能的依赖项
App_Link_Flags := -L$(SGX_LIBRARY_PATH) -l$(Urts_Library_Name) -lpthread -lcoproto

App_Cpp_Objects := $(App_Cpp_Files:.cpp=.o)

App_Name := app

######## Enclave Settings ########

ifneq ($(SGX_MODE), HW)
    Trts_Library_Name := sgx_trts_sim
    Service_Library_Name := sgx_tservice_sim
else
    Trts_Library_Name := sgx_trts
    Service_Library_Name := sgx_tservice
endif
Crypto_Library_Name := sgx_tcrypto

# 【修改4】确保包含 Enclave.cpp
Enclave_Cpp_Files := Enclave/Enclave.cpp
Enclave_Include_Paths := -IInclude -IEnclave -I$(SGX_SDK)/include -I$(SGX_SDK)/include/tlibc -I$(SGX_SDK)/include/libcxx

Enclave_C_Flags := $(Enclave_Include_Paths) -nostdinc -fvisibility=hidden -fpie -ffunction-sections -fdata-sections
CC_BELOW_4_9 := $(shell expr "`$(CC) -dumpversion`" \< "4.9")
ifeq ($(CC_BELOW_4_9), 1)
    Enclave_C_Flags += -fstack-protector
else
    Enclave_C_Flags += -fstack-protector-strong
endif

Enclave_Cpp_Flags := $(Enclave_C_Flags) -nostdinc++

Enclave_Security_Link_Flags := -Wl,-z,relro,-z,now,-z,noexecstack

Enclave_Link_Flags := $(Enclave_Security_Link_Flags) \
    -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L$(SGX_LIBRARY_PATH) \
    -Wl,--whole-archive -l$(Trts_Library_Name) -Wl,--no-whole-archive \
    -Wl,--start-group -lsgx_tstdc -lsgx_tcxx -l$(Crypto_Library_Name) -l$(Service_Library_Name) -Wl,--end-group \
    -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
    -Wl,-pie,-eenclave_entry -Wl,--export-dynamic \
    -Wl,--defsym,__ImageBase=0 -Wl,--gc-sections \
    -Wl,--version-script=Enclave/Enclave.lds

Enclave_Cpp_Objects := $(Enclave_Cpp_Files:.cpp=.o)

Enclave_Name := enclave.so
Signed_Enclave_Name := enclave.signed.so
Enclave_Config_File := Enclave/Enclave.config.xml
Enclave_Test_Key := Enclave/Enclave_private_test.pem

######## 编译目标逻辑 ########

.PHONY: all target run clean

all: .config_$(SGX_MODE)_$(SGX_DEBUG)
	@$(MAKE) target

target: $(App_Name) $(Signed_Enclave_Name)

.config_$(SGX_MODE)_$(SGX_DEBUG):
	@rm -f .config_* $(App_Name) $(Enclave_Name) $(Signed_Enclave_Name) $(App_Cpp_Objects) App/Enclave_u.* $(Enclave_Cpp_Objects) Enclave/Enclave_t.*
	@touch .config_$(SGX_MODE)_$(SGX_DEBUG)

######## App Objects ########

App/Enclave_u.h: $(SGX_EDGER8R) Enclave/Enclave.edl
	@cd App && $(SGX_EDGER8R) --untrusted ../Enclave/Enclave.edl --search-path ../Enclave --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

App/Enclave_u.c: App/Enclave_u.h

App/Enclave_u.o: App/Enclave_u.c
	@$(CC) $(SGX_COMMON_CFLAGS) $(App_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"

App/%.o: App/%.cpp App/Enclave_u.h
	@$(CXX) $(SGX_COMMON_CXXFLAGS) $(App_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"

$(App_Name): App/Enclave_u.o $(App_Cpp_Objects)
	@$(CXX) $^ -o $@ $(App_Link_Flags)
	@echo "LINK =>  $@"

######## Enclave Objects ########

Enclave/Enclave_t.h: $(SGX_EDGER8R) Enclave/Enclave.edl
	@cd Enclave && $(SGX_EDGER8R) --trusted ../Enclave/Enclave.edl --search-path ../Enclave --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

Enclave/Enclave_t.c: Enclave/Enclave_t.h

Enclave/Enclave_t.o: Enclave/Enclave_t.c
	@$(CC) $(SGX_COMMON_CFLAGS) $(Enclave_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"

Enclave/%.o: Enclave/%.cpp Enclave/Enclave_t.h
	@$(CXX) $(SGX_COMMON_CXXFLAGS) $(Enclave_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"

$(Enclave_Name): Enclave/Enclave_t.o $(Enclave_Cpp_Objects)
	@$(CXX) $^ -o $@ $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

$(Signed_Enclave_Name): $(Enclave_Name)
	@if [ ! -f $(Enclave_Test_Key) ]; then openssl genrsa -out $(Enclave_Test_Key) -3 3072; fi
	@$(SGX_ENCLAVE_SIGNER) sign -key $(Enclave_Test_Key) -enclave $(Enclave_Name) -out $@ -config $(Enclave_Config_File)
	@echo "SIGN =>  $@"

clean:
	@rm -f .config_* $(App_Name) $(Enclave_Name) $(Signed_Enclave_Name) $(App_Cpp_Objects) App/Enclave_u.* $(Enclave_Cpp_Objects) Enclave/Enclave_t.* $(Enclave_Test_Key)