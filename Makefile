objects = main.o symbolicexecutor.o ProgramState.o llvmExpressionTree.o utils.o jsoncpp.o Socket.o ServerSocket.o JsonReader.o
objectPaths = $(shell find build/*.o)
llvmPath = /usr/lib/llvm-3.5/build/include
INCLUDES = -I$(llvmPath)

llvmFlags = `llvm-config-3.5 --libs core jit native` `llvm-config-3.5 --cxxflags --ldflags --libs` `llvm-config --libs core` `llvm-config --ldflags`  -ltinfo -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -ldl
exceptionFlag = -fexceptions
z3Flag = -lz3
multithreadingFlag = -pthread
BUILD_DIR = build
SRC_DIR = src
VPATH = $(SRC_DIR):$(SRC_DIR)/server:$(SRC_DIR)/jsoncpp/dist:$(SRC_DIR)/jsoncpp/dist/json:$(BUILD_DIR):$(SRC_DIR)/SUT
CXX = g++-4.8 -std=c++0x
PREFIX = $(CXX) $(SRC_DIR)/
SUFFIX = $(BUILD_DIR)/$@ 
CPPFLAGS = -g


all: hello.bc sfse

debug: CPPFLAGS += -DDEBUG
debug: all

sfse: $(objects)
	$(CXX) $(CPPFLAGS) $(objectPaths) $(INCLUDES) -o sfse $(llvmFlags) $(exceptionFlag) $(z3Flag) $(multithreadingFlag) 

main.o: main.cpp symbolicexecutor.h SocketException.h
	$(PREFIX)main.cpp $(CPPFLAGS) $(INCLUDES) -c -o $(SUFFIX)

Socket.o: Socket.h Socket.cpp
	$(PREFIX)server/Socket.cpp $(CPPFLAGS) -c -o $(SUFFIX)

ServerSocket.o: ServerSocket.h ServerSocket.cpp Socket.h SocketException.h
	$(PREFIX)server/ServerSocket.cpp $(CPPFLAGS) -c -o $(SUFFIX)

symbolicexecutor.o: symbolicexecutor.h symbolicexecutor.cpp json.h llvmExpressionTree.h ProgramState.h
	$(PREFIX)symbolicexecutor.cpp $(INCLUDES) $(CPPFLAGS) -c -o $(SUFFIX)
ProgramState.o: ProgramState.h ProgramState.cpp llvmExpressionTree.h
	$(PREFIX)ProgramState.cpp $(INCLUDES) $(CPPFLAGS) -c -o $(SUFFIX)
llvmExpressionTree.o: utils.h llvmExpressionTree.h llvmExpressionTree.cpp
	$(PREFIX)llvmExpressionTree.cpp $(INCLUDES) $(CPPFLAGS) -c -o $(SUFFIX)
utils.o: utils.h utils.cpp
	$(PREFIX)utils.cpp $(INCLUDES) $(CPPFLAGS) -c -o $(SUFFIX)
hello.bc: hello.cpp
	clang-3.5 -emit-llvm $(SRC_DIR)/SUT/hello.cpp -g -c -o $(SUFFIX)
	llvm-dis-3.5 $(BUILD_DIR)/hello.bc -o $(BUILD_DIR)/humanreadable_hello.bc
jsoncpp.o: jsoncpp.cpp json.h
	$(PREFIX)jsoncpp/dist/jsoncpp.cpp -c -o $(SUFFIX)
JsonReader.o: jsonreader.h jsonreader.cpp llvmExpressionTree.h ServerSocket.h json.h utils.h
	$(PREFIX)jsonreader.cpp $(INCLUDES) $(CPPFLAGS) -c -o $(SUFFIX)


setup:
	@mkdir -p build
clean:
	@rm -f $(BUILD_DIR)/*.o