PY ?= python3
export PYTHONPATH := src

.PHONY: test demo calibrate-demo clean

test:
	$(PY) -m pytest -q

demo:
	$(PY) -m gplab demo --out out/demo

calibrate-demo: demo
	$(PY) -m gplab holdout --bars out/demo/bars --config config/pilot_house.yaml --out out/demo/calibration --seeds 10

clean:
	rm -rf out .pytest_cache
