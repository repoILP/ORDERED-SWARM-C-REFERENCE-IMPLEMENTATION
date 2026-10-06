# Ordered Swarm — DAG / Partial-Order Experiment

Reference C implementation for studying an ordered processing swarm.

## Build

```bash
cc -std=c11 -Wall -Wextra -Wpedantic -O2 ordered_swarm.c -o ordered_swarm
```

## Run

```bash
./ordered_swarm
```

The program demonstrates:

- DAG validation;
- topological sorting;
- execution levels;
- structural parallelism;
- deterministic propagation;
- cycle rejection.

See [`ANALYSIS.md`](ANALYSIS.md) for the scientific comparison between
ordered DAG execution and cyclic/recurrent graphs.

> This is a structural reference experiment, not a hardware benchmark.
