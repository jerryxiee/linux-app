CC              := mipsel-openwrt-linux-uclibc-gcc
CXX             := mipsel-openwrt-linux-uclibc-g++

SELF_INC        := -I./inc \
                   -I./inc/p2p \
                   -I./inc/curl \
                   -I./inc/mp4v2 \
		   -I./inc/cloud \
                   -I/home/openwrt/OpenWrt-SDK-ramips-mt7688_gcc-4.8-linaro_uClibc-0.9.33.2.Linux-x86_64/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/include
                 
INC_PATH        :=  $(SELF_INC)

PRE_LIB	        := -L./lib 
LIB_PATH        := $(PRE_LIB)

LIB	        :=  -lasound -le2rom -lPPCS_API -lcrypto -lssl -lcurl -lmp4v2 -lmfsdk -lpthread -lstdc++ -lrt -ldl -lm 

MODULE_DEFINES  := -DLINUX
CFLAGS	        := $(MODULE_DEFINES)  -Wall -O2 -o 

TARGET          := Ppcs_vava
SRCC	        := $(wildcard *.c)
COBJ	        := $(SRCC:.c=.o)
PRESRCC	        := $(wildcard ./src/*.c)
PRECOBJ	        := $(PRESRCC:.c=.o)
SRCXX	        := $(wildcard *.cpp)
CXXOBJ         := $(SRCXX:.cpp=.o)
OBJ	        := $(COBJ)  $(PRECOBJ) $(OBJ)

$(TARGET):$(OBJ)
	$(CC) $(INC_PATH) $(LIB_PATH) $(CFLAGS) $@ $(OBJ) $(LIB)

$(COBJ):%.o:%.c 
	$(CC) $(INC_PATH) $(CFLAGS) $@ -c $<

$(PRECOBJ):%.o:%.c 
	$(CC) $(INC_PATH) $(CFLAGS) $@ -c $<

$(CXXOBJ):%.o:%.cpp 
	$(CXX) $(INC_PATH) $(CFLAGS) $@ -c $<

.PHONY:clean
clean:
	rm -f $(TARGET) *~ ./src/*.o 
