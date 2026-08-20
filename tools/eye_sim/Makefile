CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pthread -I../../

TARGET = eye_sim

all: $(TARGET)

$(TARGET): eye_sim.cpp ../../src/eye_pair_sync/eye_output_mode.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

sim: $(TARGET)
	./$(TARGET)

sim-logs:
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all sim sim-logs clean
