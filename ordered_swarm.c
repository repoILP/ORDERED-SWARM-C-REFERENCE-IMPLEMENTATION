/*
 * ordered_swarm.c
 *
 * Experimental implementation of an ordered processing swarm.
 *
 * Model:
 *   - Vertices represent processing units.
 *   - Directed edges represent precedence constraints.
 *   - A valid structure is a DAG.
 *   - Reachability induces a partial order over the vertices.
 *   - Topological levels expose structurally independent work.
 *
 * Build:
 *   cc -std=c11 -Wall -Wextra -Wpedantic -O2 ordered_swarm.c -o ordered_swarm
 *
 * Run:
 *   ./ordered_swarm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NODES 32
#define NAME_LEN  32

typedef struct {
    char name[NAME_LEN];
    double weight;
} Node;

typedef struct {
    size_t count;
    Node nodes[MAX_NODES];
    unsigned char edge[MAX_NODES][MAX_NODES];
} Swarm;

static void die(const char *message)
{
    fprintf(stderr, "error: %s\n", message);
    exit(EXIT_FAILURE);
}

static int find_node(const Swarm *swarm, const char *name)
{
    for (size_t i = 0; i < swarm->count; ++i) {
        if (strcmp(swarm->nodes[i].name, name) == 0)
            return (int)i;
    }

    return -1;
}

static size_t add_node(Swarm *swarm, const char *name, double weight)
{
    if (swarm->count >= MAX_NODES)
        die("maximum number of nodes reached");

    if (find_node(swarm, name) >= 0)
        die("duplicate node name");

    size_t id = swarm->count++;

    snprintf(swarm->nodes[id].name, NAME_LEN, "%s", name);
    swarm->nodes[id].weight = weight;

    return id;
}

static void add_edge_by_name(Swarm *swarm,
                             const char *before,
                             const char *after)
{
    int u = find_node(swarm, before);
    int v = find_node(swarm, after);

    if (u < 0 || v < 0)
        die("edge references an unknown node");

    swarm->edge[u][v] = 1;
}

/*
 * Kahn's algorithm.
 *
 * Returns:
 *   1 -> DAG
 *   0 -> cycle detected
 *
 * If order != NULL, it receives the topological ordering.
 */
static int topological_sort(const Swarm *swarm,
                            size_t order[MAX_NODES])
{
    size_t indegree[MAX_NODES] = {0};
    size_t queue[MAX_NODES];
    size_t head = 0;
    size_t tail = 0;
    size_t produced = 0;

    for (size_t u = 0; u < swarm->count; ++u) {
        for (size_t v = 0; v < swarm->count; ++v) {
            if (swarm->edge[u][v])
                ++indegree[v];
        }
    }

    for (size_t i = 0; i < swarm->count; ++i) {
        if (indegree[i] == 0)
            queue[tail++] = i;
    }

    while (head < tail) {
        size_t u = queue[head++];

        if (order != NULL)
            order[produced] = u;

        ++produced;

        for (size_t v = 0; v < swarm->count; ++v) {
            if (!swarm->edge[u][v])
                continue;

            --indegree[v];

            if (indegree[v] == 0)
                queue[tail++] = v;
        }
    }

    return produced == swarm->count;
}

/*
 * Computes the longest-path level from any source.
 *
 * Nodes with the same level have no precedence relation caused by
 * the constructed dependency depth. They form a valid execution
 * layer for this scheduler.
 */
static void compute_levels(const Swarm *swarm,
                            const size_t order[MAX_NODES],
                            size_t level[MAX_NODES])
{
    for (size_t i = 0; i < swarm->count; ++i)
        level[i] = 0;

    for (size_t i = 0; i < swarm->count; ++i) {
        size_t u = order[i];

        for (size_t v = 0; v < swarm->count; ++v) {
            if (!swarm->edge[u][v])
                continue;

            if (level[v] < level[u] + 1)
                level[v] = level[u] + 1;
        }
    }
}

/*
 * Simple deterministic dataflow evaluation.
 *
 * Each node:
 *   - uses 1.0 if it has no predecessors;
 *   - otherwise sums predecessor values;
 *   - multiplies the result by its node weight.
 */
static void execute(const Swarm *swarm,
                    const size_t order[MAX_NODES],
                    double values[MAX_NODES])
{
    for (size_t i = 0; i < swarm->count; ++i)
        values[i] = 0.0;

    for (size_t i = 0; i < swarm->count; ++i) {
        size_t u = order[i];
        double input = 0.0;
        int has_predecessor = 0;

        for (size_t p = 0; p < swarm->count; ++p) {
            if (!swarm->edge[p][u])
                continue;

            input += values[p];
            has_predecessor = 1;
        }

        if (!has_predecessor)
            input = 1.0;

        values[u] = input * swarm->nodes[u].weight;
    }
}

static void print_order(const Swarm *swarm,
                        const size_t order[MAX_NODES])
{
    printf("topological order:\n  ");

    for (size_t i = 0; i < swarm->count; ++i) {
        printf("%s%s",
               swarm->nodes[order[i]].name,
               (i + 1 == swarm->count) ? "\n" : " -> ");
    }
}

static void print_levels(const Swarm *swarm,
                         const size_t level[MAX_NODES])
{
    size_t max_level = 0;

    for (size_t i = 0; i < swarm->count; ++i) {
        if (level[i] > max_level)
            max_level = level[i];
    }

    size_t max_width = 0;

    printf("execution levels:\n");

    for (size_t l = 0; l <= max_level; ++l) {
        size_t width = 0;

        printf("  level %zu: ", l);

        for (size_t i = 0; i < swarm->count; ++i) {
            if (level[i] != l)
                continue;

            printf("%s%s",
                   width++ == 0 ? "" : ", ",
                   swarm->nodes[i].name);
        }

        printf("\n");

        if (width > max_width)
            max_width = width;
    }

    printf("maximum structural parallelism: %zu node(s)\n", max_width);
}

static void print_values(const Swarm *swarm,
                         const size_t order[MAX_NODES],
                         const double values[MAX_NODES])
{
    printf("deterministic propagation:\n");

    for (size_t i = 0; i < swarm->count; ++i) {
        size_t u = order[i];
        printf("  %-12s -> %.3f\n",
               swarm->nodes[u].name,
               values[u]);
    }
}

static void build_reference_swarm(Swarm *swarm)
{
    memset(swarm, 0, sizeof(*swarm));

    add_node(swarm, "sensor_A",   1.0);
    add_node(swarm, "sensor_B",   1.0);
    add_node(swarm, "sensor_C",   1.0);
    add_node(swarm, "process_A",  1.2);
    add_node(swarm, "process_B",  0.8);
    add_node(swarm, "fusion",     1.5);
    add_node(swarm, "decision",   1.0);
    add_node(swarm, "actuator_A", 1.0);
    add_node(swarm, "actuator_B", 1.0);

    add_edge_by_name(swarm, "sensor_A", "process_A");
    add_edge_by_name(swarm, "sensor_B", "process_A");

    add_edge_by_name(swarm, "sensor_B", "process_B");
    add_edge_by_name(swarm, "sensor_C", "process_B");

    add_edge_by_name(swarm, "process_A", "fusion");
    add_edge_by_name(swarm, "process_B", "fusion");

    add_edge_by_name(swarm, "fusion", "decision");

    add_edge_by_name(swarm, "decision", "actuator_A");
    add_edge_by_name(swarm, "decision", "actuator_B");
}

static void test_cycle_detection(void)
{
    Swarm cyclic;

    build_reference_swarm(&cyclic);
    add_edge_by_name(&cyclic, "actuator_A", "process_A");

    if (topological_sort(&cyclic, NULL)) {
        printf("[FAIL] cycle detection\n");
        exit(EXIT_FAILURE);
    }

    printf("[PASS] cycle detection\n");
}

static void test_reference_swarm(void)
{
    Swarm swarm;
    size_t order[MAX_NODES];
    size_t level[MAX_NODES];
    double values[MAX_NODES];

    build_reference_swarm(&swarm);

    if (!topological_sort(&swarm, order)) {
        printf("[FAIL] reference graph must be a DAG\n");
        exit(EXIT_FAILURE);
    }

    compute_levels(&swarm, order, level);
    execute(&swarm, order, values);

    print_order(&swarm, order);
    print_levels(&swarm, level);
    print_values(&swarm, order, values);

    /*
     * Expected structural properties:
     *   - 9 nodes
     *   - 5 levels
     *   - maximum width = 3
     *   - fusion = (2.4 + 1.6) * 1.5 = 6.0
     */
    int fusion = find_node(&swarm, "fusion");

    if (fusion < 0 || values[fusion] != 6.0) {
        printf("[FAIL] deterministic propagation result\n");
        exit(EXIT_FAILURE);
    }

    printf("[PASS] deterministic propagation\n");
}

int main(void)
{
    printf("============================================================\n");
    printf("ORDERED SWARM — C REFERENCE IMPLEMENTATION\n");
    printf("============================================================\n\n");

    printf("[TEST] reference DAG\n");
    test_reference_swarm();

    printf("\n[TEST] cycle rejection\n");
    test_cycle_detection();

    printf("\nAll tests passed.\n");

    return EXIT_SUCCESS;
}
