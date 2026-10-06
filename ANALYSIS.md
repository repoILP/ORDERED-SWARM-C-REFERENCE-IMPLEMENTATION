# Scientific Analysis — Ordered Swarm vs. Cyclic Graph

## 1. Scope

This document evaluates the consequences of imposing an ordered, acyclic
execution model on a graph that could otherwise contain arbitrary feedback.

The comparison is deliberately scientific:

- no assumption that one architecture is universally superior;
- separate **observed properties** from theoretical advantages;
- distinguish structural guarantees from implementation-dependent performance;
- identify both gains and losses;
- avoid treating mathematical acyclicity as a guarantee of overall system
  correctness.

The accompanying C implementation is a small reference experiment. It is not
a benchmark of CPU/GPU performance and should not be interpreted as one.

---

## 2. Mathematical model

Let the processing structure be a directed graph:

\[
G=(V,E)
\]

where:

- \(V\) is the set of processing units;
- \(E\) contains precedence constraints;
- \(u\rightarrow v\) means that \(u\) must precede \(v\).

If \(G\) is acyclic, it is a DAG.

The DAG induces a partial order through **reachability**:

\[
u\preceq v
\]

when either \(u=v\), or there exists a directed path from \(u\) to \(v\).

This distinction matters:

> The DAG's immediate edges are not themselves necessarily a complete
> partial-order relation. The partial order is obtained from reachability
> (with reflexivity included).

The resulting relation is:

1. reflexive;
2. antisymmetric;
3. transitive.

Therefore, the reachable structure forms a poset.

---

## 3. Reference experiment

The C program constructs this dependency graph:

```text
sensor_A ──┐
           ├──> process_A ──┐
sensor_B ──┘                │
           ├──> process_B ──┤──> fusion ──> decision ──┬──> actuator_A
sensor_C ──┘                                             └──> actuator_B
```

The graph contains:

- 9 nodes;
- 9 dependency edges;
- 5 execution levels;
- maximum structural width of 3 nodes.

The levels are:

```text
Level 0: sensor_A, sensor_B, sensor_C
Level 1: process_A, process_B
Level 2: fusion
Level 3: decision
Level 4: actuator_A, actuator_B
```

The maximum structural parallelism is therefore:

\[
P_{\max}=3
\]

This means three operations are structurally independent at the widest level.
It does **not** imply that three CPU cores or GPU lanes will necessarily
produce a 3× speedup.

---

## 4. Observed properties

The reference implementation verifies the following:

### 4.1 Topological ordering

Kahn's algorithm produces an ordering in which every dependency appears
before its dependent node.

For the reference graph, one valid ordering is:

```text
sensor_A
→ sensor_B
→ sensor_C
→ process_A
→ process_B
→ fusion
→ decision
→ actuator_A
→ actuator_B
```

A topological ordering is not necessarily unique.

---

### 4.2 Cycle detection

The experiment introduces:

```text
actuator_A → process_A
```

which closes a cycle:

```text
process_A
→ fusion
→ decision
→ actuator_A
→ process_A
```

The topological sort rejects the graph.

This provides a structural guarantee:

> A cycle in the dependency graph can be detected before executing the
> ordered dataflow.

The guarantee is specifically about graph cycles. It is not a universal
guarantee against every possible software deadlock.

---

### 4.3 Deterministic propagation

Each node computes:

\[
x_v =
\begin{cases}
w_v, & \text{if }v\text{ has no predecessors}\\
w_v\sum_{u\rightarrow v}x_u, & \text{otherwise}
\end{cases}
\]

For the reference graph:

```text
process_A = (1 + 1) × 1.2 = 2.4
process_B = (1 + 1) × 0.8 = 1.6

fusion = (2.4 + 1.6) × 1.5 = 6.0

decision   = 6.0
actuator_A = 6.0
actuator_B = 6.0
```

The result is deterministic for a fixed graph and fixed inputs.

---

# 5. Gains of the ordered/DAG model

## 5.1 Explicit precedence

The main structural gain is that dependencies become explicit.

Instead of asking the runtime to discover or simulate temporal interactions,
the graph states:

```text
A must happen before B.
```

This makes dependency analysis considerably easier.

---

## 5.2 Topological scheduling

A DAG admits a topological ordering.

For \(V\) vertices and \(E\) edges, standard topological sorting can be
performed in:

\[
O(|V|+|E|)
\]

This is useful for:

- static scheduling;
- dependency validation;
- build systems;
- computation graphs;
- compiler pipelines;
- dataflow systems;
- task graphs.

---

## 5.3 Structural parallelism

Nodes without precedence relationships can potentially be scheduled
concurrently.

The concept is closely related to antichains.

In the reference graph:

```text
sensor_A
sensor_B
sensor_C
```

form the widest independent layer.

Likewise:

```text
process_A
process_B
```

can be executed concurrently.

This exposes parallelism before execution.

### Important qualification

Structural independence is not sufficient to guarantee safe parallel execution.

There may also be:

- shared-memory dependencies;
- external I/O;
- hidden side effects;
- resource contention;
- synchronization constraints.

Therefore:

\[
\text{graph independence}
\neq
\text{complete race-freedom proof}
\]

---

## 5.4 Finite structural propagation

A finite DAG contains no directed cycle.

Consequently, an execution that follows every node according to a topological
schedule cannot continue indefinitely by repeatedly traversing the same
dependency cycle.

This is a strong termination property for the **graph traversal itself**.

It does not prove termination of arbitrary code executed by a node.

For example, a node could still contain:

```c
while (1) {
    /* infinite computation */
}
```

The DAG cannot prevent that.

---

## 5.5 Static optimization

An acyclic dependency structure is amenable to optimizations such as:

- dead-node elimination;
- dependency pruning;
- constant propagation;
- common-subexpression analysis;
- task fusion;
- level scheduling;
- memory-lifetime analysis;
- pipeline construction.

These are easier when future dependencies cannot feed information backward into
already executed nodes.

---

## 5.6 Hardware mapping

A DAG can be interpreted as a task/dataflow graph:

```text
CPU core 0 ── task A ──┐
CPU core 1 ── task B ──┼── task C
CPU core 2 ── task D ──┘
```

The scheduler can map independent tasks onto available resources.

This is particularly relevant to:

- multicore CPUs;
- GPUs;
- FPGAs;
- ASIC dataflow accelerators;
- heterogeneous computing.

However, the actual performance depends on graph width, task granularity,
synchronization overhead, memory bandwidth and hardware utilization.

---

# 6. Losses of the ordered/DAG model

## 6.1 Loss of natural feedback

A cyclic system can represent:

```text
A → B → C
    ↑   |
    └───┘
```

The output can influence a future state.

A pure DAG cannot represent this recurrence inside the same graph.

This is a fundamental expressive difference.

---

## 6.2 Loss of implicit temporal memory

A recurrent system can naturally implement:

\[
x_{t+1}=f(x_t)
\]

where the current state influences the next state.

A DAG naturally represents something closer to:

\[
y=f(x)
\]

If temporal memory is required in the DAG model, it must normally be represented
explicitly outside the acyclic dependency relation.

Therefore, the DAG moves memory from:

> an emergent property of recurrence

toward:

> an explicit state-management mechanism.

---

## 6.3 Loss of oscillatory dynamics

Cycles permit behaviors such as:

```text
A → B → C → A
```

which can produce:

- oscillation;
- attractors;
- recurrent patterns;
- feedback stabilization;
- unstable growth;
- temporal competition.

A pure DAG eliminates these behaviors from the dependency topology.

This is not merely a performance loss. It is a reduction in the class of
dynamics naturally representable by the graph.

---

## 6.4 Reduced topological plasticity

Suppose a valid DAG contains:

```text
A → B
```

Adding:

```text
B → A
```

is prohibited because it creates a cycle.

A recurrent architecture can accept such a structural modification and acquire a
new feedback loop.

Therefore, a strict DAG imposes a stronger constraint on structural adaptation.

---

# 7. Scientific comparison

| Property | Cyclic/recursive graph | Ordered DAG |
|---|---:|---:|
| Feedback | Strong | Absent inside DAG |
| Temporal memory | Natural | Explicitly added |
| Oscillation | Possible | Not generated by graph cycles |
| Topological ordering | No global ordering | Yes |
| Static scheduling | Harder | Easier |
| Structural parallelism | Harder to expose | Easier to expose |
| Deterministic dataflow | Possible but harder | Natural |
| Cycle detection | Not a failure condition | Required/available |
| Finite graph traversal | Not sufficient for termination | Guaranteed for traversal |
| Formal dependency proofs | Harder | Easier |
| Hardware task mapping | More difficult | More direct |
| Dynamic feedback | Strong | Restricted |
| Topological plasticity | Higher | Lower |
| Static optimization | More constrained | More accessible |
| Expressive temporal dynamics | Higher | Lower |

---

# 8. Important negative conclusions

Several statements should **not** be inferred from the DAG model.

### DAG does not imply higher performance

A graph can be acyclic and completely sequential:

```text
A → B → C → D → E → F
```

Its parallelism is effectively zero.

---

### DAG does not eliminate all deadlocks

The graph can be acyclic while independent threads deadlock on locks,
resources or external systems.

The DAG eliminates circular dependency at the graph level, not every possible
concurrency failure.

---

### DAG does not prove semantic correctness

A valid topological ordering only proves that the ordering respects the
declared dependencies.

It does not prove that the computation itself is correct.

---

### Antichain width does not equal speedup

If:

\[
W=3
\]

then at most three tasks are structurally independent at that level.

It does not follow that:

\[
T_{parallel}=T_{serial}/3
\]

because real execution includes:

- scheduling overhead;
- communication;
- synchronization;
- memory access;
- load imbalance;
- hardware limitations.

---

# 9. The central trade-off

The transformation can be summarized as:

\[
\boxed{
\text{dynamic freedom}
\longleftrightarrow
\text{structural control}
}
\]

More specifically:

### DAG gains

\[
\boxed{
\text{precedence}
+
\text{predictability}
+
\text{parallelism exposure}
+
\text{static analysis}
+
\text{optimization}
}
\]

### DAG losses

\[
\boxed{
\text{feedback}
+
\text{implicit temporal memory}
+
\text{oscillatory dynamics}
+
\text{some structural plasticity}
}
\]

Neither side dominates universally.

The appropriate architecture depends on whether the problem is primarily a
**dataflow problem** or a **dynamic-state problem**.

---

# 10. Hybrid architecture

The most interesting experimental direction is therefore not necessarily
"convert the entire swarm into a DAG."

A hybrid architecture can separate:

```text
                 SWARM
                   |
          +--------+--------+
          |                 |
          v                 v
    ACYCLIC REGIONS     RECURRENT REGIONS
          |                 |
          v                 v
       DAG mode         dynamic mode
          |                 |
          v                 v
     static/task       temporal state
      scheduling        + feedback
```

A possible scheduler could:

1. identify strongly connected components;
2. collapse each acyclic region into a DAG;
3. execute independent DAG regions in parallel;
4. preserve strongly connected components as recurrent subsystems;
5. synchronize only at explicit boundaries.

This could preserve some of the temporal expressiveness of recurrence while
recovering static scheduling opportunities wherever the topology permits it.

---

# 11. What should be measured experimentally

A rigorous future comparison should measure, rather than assume:

### Performance

- total execution time;
- throughput;
- latency;
- scheduler overhead;
- CPU utilization;
- GPU utilization.

### Parallelism

- DAG width;
- average antichain width;
- critical-path length;
- number of simultaneously executable tasks;
- synchronization frequency.

### Memory

- peak memory;
- intermediate-state count;
- state lifetime;
- memory bandwidth.

### Dynamic behavior

For recurrent systems:

- convergence time;
- oscillation frequency;
- attractor count;
- stability;
- sensitivity to perturbations.

### Adaptation

- number of accepted topology changes;
- number of rejected changes due to cycles;
- structural reconfiguration cost.

### Correctness

- invariant preservation;
- dependency violations;
- race conditions;
- numerical divergence;
- failed executions.

Only these measurements can establish whether an ordered implementation is
actually advantageous for a specific workload.

---

# 12. Conclusion

The scientific conclusion is not:

> "DAGs are better than cyclic graphs."

The defensible conclusion is:

> A DAG converts part of the problem from dynamic dependency resolution into
> explicit structural scheduling.

This produces real advantages in:

- predictability;
- topological analysis;
- static optimization;
- dependency validation;
- parallelism discovery;
- hardware scheduling.

The corresponding cost is a reduction in the natural representation of:

- feedback;
- temporal memory;
- recurrence;
- oscillatory behavior;
- certain forms of structural adaptation.

Therefore, the most promising architecture for a general-purpose swarm is
potentially a **hybrid graph**, in which acyclic regions are exploited as
ordered dataflow and genuinely temporal regions retain recurrence.

The reference C implementation establishes the basic mechanism required for
that investigation: DAG construction, topological validation, cycle rejection,
level extraction, structural parallelism measurement and deterministic
propagation.
