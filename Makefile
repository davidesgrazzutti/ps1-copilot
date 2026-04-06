TARGET = snake_ps1

# Imposta queste variabili in base alla tua installazione, esempio:
# set PSN00BSDK_ROOT=C:/psn00bsdk
# set PSN00BSDK_INCLUDE=C:/psn00bsdk/include/libpsn00b
# set PSN00BSDK_LIBS=C:/psn00bsdk/lib/libpsn00b/release

PREFIX ?= mipsel-none-elf-
CC     := $(PREFIX)gcc
LD     := $(PREFIX)gcc
OBJCOPY:= $(PREFIX)objcopy
ELF2X  := elf2x

PSN00BSDK_ROOT ?= C:/psn00bsdk
PSN00BSDK_INCLUDE ?= $(PSN00BSDK_ROOT)/include/libpsn00b
PSN00BSDK_LIBS ?= $(PSN00BSDK_ROOT)/lib/libpsn00b/release
PSN00BSDK_LDSCRIPT ?= $(PSN00BSDK_ROOT)/lib/libpsn00b/ldscripts/exe.ld

CFLAGS  := -O2 -G0 -Wall -Wextra -Wa,--strip-local-absolute -ffreestanding -fno-builtin -nostdlib
CFLAGS  += -fdata-sections -ffunction-sections -fsigned-char -fno-strict-overflow
CFLAGS  += -msoft-float -march=r3000 -mtune=r3000 -mabi=32 -mno-mt -mno-llsc
CFLAGS  += -fno-pic -mno-abicalls -mno-gpopt
CFLAGS  += -Iinclude -I$(PSN00BSDK_INCLUDE)
LDFLAGS := -nostdlib -Wl,-gc-sections -G0 -static -T$(PSN00BSDK_LDSCRIPT) -Wl,-Map=$(TARGET).map
LDFLAGS += -L$(PSN00BSDK_LIBS)
LDFLAGS += -lpsxgpu_exe_nogprel -lpsxgte_exe_nogprel -lpsxspu_exe_nogprel -lpsxcd_exe_nogprel
LDFLAGS += -lpsxpress_exe_nogprel -lpsxsio_exe_nogprel -lpsxetc_exe_nogprel -lpsxapi_exe_nogprel
LDFLAGS += -lsmd_exe_nogprel -llzp_exe_nogprel -lc_exe_nogprel -lgcc

SRC = src/main.c
OBJ = $(SRC:.c=.o)

all: $(TARGET).elf $(TARGET).exe $(TARGET).bin

$(TARGET).elf: $(OBJ)
	$(LD) -o $@ $^ $(LDFLAGS)

$(TARGET).exe: $(TARGET).elf
	$(ELF2X) -q $< $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	-del /Q src\*.o 2>nul
	-del /Q $(TARGET).elf 2>nul
	-del /Q $(TARGET).exe 2>nul
	-del /Q $(TARGET).bin 2>nul
	-del /Q $(TARGET).map 2>nul

.PHONY: all clean
