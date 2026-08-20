.PHONY: sim sim-logs clean

sim:
	$(MAKE) -C tools/eye_sim sim

sim-logs:
	$(MAKE) -C tools/eye_sim sim-logs

clean:
	$(MAKE) -C tools/eye_sim clean
