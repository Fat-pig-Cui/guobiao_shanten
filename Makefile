CXX = c++
CXXFLAGS ?= -O3 -std=c++17
PYTHON ?= python3

PROGRAMS = compute_joint test_joint compute_all_not_connected compute_combo_dragon compute_n14_tianhe sample_best14
WEIGHTS = joint_distribution.txt base_distribution.txt base_anc_distribution.txt base_combo_distribution.txt all_not_connected_distribution.txt combo_dragon_distribution.txt

.PHONY: all test report check-report

all: $(PROGRAMS)

$(PROGRAMS): %: %.cpp shanten_core.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDFLAGS) -pthread $(LDLIBS) -o $@

compute_joint test_joint: guobiao_engine.hpp

test: test_joint check-report
	./test_joint

report: report_template.md make_report_tables.py $(WEIGHTS)
	$(PYTHON) make_report_tables.py --render report_template.md report.md

check-report: report_template.md make_report_tables.py $(WEIGHTS)
	@set -eu; generated=$$(mktemp); trap 'rm -f "$$generated"' EXIT HUP INT TERM; \
	$(PYTHON) make_report_tables.py --render report_template.md "$$generated"; \
	cmp report.md "$$generated"
