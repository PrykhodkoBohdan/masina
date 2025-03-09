
CXX = arm-linux-gnueabihf-g++
CXXFLAGS = -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -flto -fdata-sections -ffunction-sections -mtune=cortex-a9
PTHREAD = -pthread

OUTPUT_DIR = drop/general/package/sigmastar-osdrv-infinity6e/files/script

TARGETS = $(OUTPUT_DIR)/masinaclient

all: $(TARGETS)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(OUTPUT_DIR)/masinaclient: client.cpp utils.cpp | $(OUTPUT_DIR)
	$(CXX) $(CXXFLAGS) client.cpp utils.cpp -o $@ $(PTHREAD) -Wl,--gc-sections

clean:
	rm -f $(OUTPUT_DIR)/masinaclient

strip: $(TARGETS)
	strip $(OUTPUT_DIR)/masinaclient

.PHONY: all clean strip