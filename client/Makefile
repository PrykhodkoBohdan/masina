# Define compiler and flags
CXX = arm-linux-gnueabihf-g++
CXXFLAGS = -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -flto -fdata-sections -ffunction-sections -mtune=cortex-a9
PTHREAD = -pthread

# Output directory
OUTPUT_DIR = drop/general/package/sigmastar-osdrv-infinity6e/files/script

# Targets
TARGETS = $(OUTPUT_DIR)/masinaclient $(OUTPUT_DIR)/masinawebserver

# Default target: build both client and masinawebserver
all: $(TARGETS)

# Ensure the output directory exists
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

# Build the client executable
$(OUTPUT_DIR)/masinaclient: client.cpp utils.cpp | $(OUTPUT_DIR)
	$(CXX) $(CXXFLAGS) client.cpp utils.cpp -o $@ $(PTHREAD) -Wl,--gc-sections

# Build the masinawebserver executable
$(OUTPUT_DIR)/masinawebserver: masinawebserver.cpp | $(OUTPUT_DIR)
	$(CXX) $(CXXFLAGS) masinawebserver.cpp -o $@ -Wl,--gc-sections

# Clean target: removes object files and executables
clean:
	rm -f $(OUTPUT_DIR)/masinaclient $(OUTPUT_DIR)/masinawebserver

# Strip executable to remove debugging symbols (if you want to further reduce size)
strip: $(TARGETS)
	strip $(OUTPUT_DIR)/masinaclient $(OUTPUT_DIR)/masinawebserver

.PHONY: all clean strip