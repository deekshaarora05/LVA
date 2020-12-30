LLVM_CONFIG=llvm-config

CXX=clang++
CXXFLAGS=`$(LLVM_CONFIG) --cppflags` -fPIC -fno-rtti
LDFLAGS=`$(LLVM_CONFIG) --ldflags`

all: LvaPass.so

LvaPass.so: LvaPass.o
	$(CXX) -shared LvaPass.o -o LvaPass.so $(LDFLAGS)

LvaPass.o: LvaPass.cpp
	$(CXX) -c LvaPass.cpp -o LvaPass.o $(CXXFLAGS)

clean:
	rm -f *.o *.so
