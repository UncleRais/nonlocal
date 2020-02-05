TARGET = a.out
HDRS = include \
	   include/finite_elements \
	   Eigen

SRC = src/main.cpp \
	  src/heat_equation_solver.cpp
	  #src/static_analysis.cpp
	  

.PHONY: all clean

all: $(SRCS)
		g++ -std=c++17 -fopenmp -O2 -Wall -Wextra $(addprefix -I,$(HDRS)) -o $(TARGET) $(CXXFLAGS) $(SRC)

clean: