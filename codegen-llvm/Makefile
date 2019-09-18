LLVM_CONFIG=/work/zhang-x1/common/install/llvm-8.0/build/bin/llvm-config
LLVM_VERSION=$(shell $(LLVM_CONFIG) --version| cut -b 1,3)
LLVM_INCLUDE=$(filter -I%, $(shell $(LLVM_CONFIG) --cxxflags))
LLVM_CFLAGS= -fno-rtti
LLVM_CFLAGS += $(LLVM_INCLUDE)
LDFLAGS += $(shell $(LLVM_CONFIG) --ldflags --libs --system-libs)

all:
	g++ $(LLVM_CFLAGS) main.cpp -o bril-llvm $(LDFLAGS)

clean:
	rm bril-llvm
