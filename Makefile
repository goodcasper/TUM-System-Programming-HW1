# Set you prefererred CFLAGS/compiler compiler here.
# Our github runner provides gcc-10 by default.
CC ?= cc
CFLAGS ?= -g -Wall -O2 -fPIC
LDFLAGS ?= -shared
# CXX ?= c++
# CXXFLAGS ?= -g -Wall -O2
# CARGO ?= cargo
# RUSTFLAGS ?= -g


# Build both libraries
all: librw_1.so librw_2.so tracer

# Task 1.1 - build the shared library
librw_1.so: task-1_1.o
	$(CC) $(LDFLAGS) -o $@ $^

# Task 1.2 - build the shared library
librw_2.so: task-1_2.o
	$(CC) $(LDFLAGS) -o $@ $^

# Compile sources
task-1_1.o: task-1_1.c
	$(CC) $(CFLAGS) -c $< -o $@

task-1_2.o: task-1_2.c
	$(CC) $(CFLAGS) -c $< -o $@

tracer: tracer.c
	$(CC) $(CFLAGS) -o tracer tracer.c


# C example:
# all: librw_1.so librw_2.so
#librw_1.so: task-1_1.c
#	$(CC) $(CFLAGS) -shared -fPIC -ldl -o $@ $<
#
#librw_2.so: task-1_2.c
#	$(CC) $(CFLAGS) -shared -fPIC -ldl -o $@ $<

# Rust example:
#all:
#	$(CARGO) build --release

# Usually there is no need to modify this
check: all
	$(MAKE) -C tests check
clean:
	rm -f *.o *.so