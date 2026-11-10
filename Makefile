# Greedy seam carving
#   make install                               install dependencies
#   make greedy IMG=images/castle.jpg WIDTH=400 HEIGHT=300
#   make greedy-multistart IMG=... WIDTH=...    greedy from every top pixel
#   make test                                  run unit tests
#   make clean                                 delete generated output

PYTHON ?= python
IMG    ?= images/input.jpg
WIDTH  ?=
HEIGHT ?=
OUT    ?= output/greedy

SIZE_ARGS = $(if $(WIDTH),--width $(WIDTH)) $(if $(HEIGHT),--height $(HEIGHT))

.PHONY: install greedy greedy-multistart test clean

install:
	$(PYTHON) -m pip install -r requirements.txt

greedy:
	$(PYTHON) -m seam_carving.greedy_cli "$(IMG)" $(SIZE_ARGS) --mode min --out-dir "$(OUT)"

greedy-multistart:
	$(PYTHON) -m seam_carving.greedy_cli "$(IMG)" $(SIZE_ARGS) --mode multistart --out-dir "$(OUT)"

test:
	$(PYTHON) -m pytest -q tests

clean:
	$(PYTHON) -c "import shutil; [shutil.rmtree(p, ignore_errors=True) for p in ('output', '.pytest_cache')]"
