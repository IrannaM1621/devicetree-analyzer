# dtdep architecture

This document describes the internal pipeline, module boundaries, and
the key design decisions made while building `dtdep`.

## Pipeline overview

```
board.dts
    |
    v
+----------------+
|  DtsLexer      |  src/parser/dts_lexer.c
|  char -> token |  Hand-written tokenizer. No regex, no external
|                |  lexer generator. Tracks line/col for diagnostics.
+-------+--------+
        | DtsToken stream
        v
+----------------+
|  dt_tree_parse |  src/model/dt_tree.c
|  token -> tree |  Recursive-descent parser. Builds DtNode / DtProp
|                |  trees. Handles labels (gcc: node {}), unit
|                |  addresses (spi@1000), and all four property value
|                |  shapes (empty, string, cell list, byte array).
+-------+--------+
        | DtTree (DtNode root + children)
        v
+----------------+
|  DtResolver    |  src/resolver/resolver.c, hash_map.c
|  &label -> node|  Two-pass: (1) index every node by label and by
|                |  declared phandle into open-addressing hash maps;
|                |  (2) walk every PROP_CELLS property and replace the
|                |  0xffffffff sentinel left by the parser with the
|                |  real phandle number, using the parallel
|                |  phandle_refs[] label string captured during
|                |  parsing.
+-------+--------+
        | DtTree with resolved phandle cells
        v
+----------------+
|  dt_dep_analyze|  src/graph/dep_engine.c
|  tree -> edges |  Walks every property on every node. Property names
|                |  are mapped to a DepType (clock, interrupt, dma,
|                |  power, gpio, reset, pinctrl, bus, phy). For each
|                |  resolved phandle cell, looks up the target node's
|                |  #xxx-cells property to know how many argument
|                |  cells follow, so argument values are never
|                |  misread as additional phandles.
+-------+--------+
        | DtDepList (flat list of typed DtDep edges)
        v
+----------------+
|  DepGraph      |  src/graph/dep_graph.c
|  edges -> graph|  Builds a fixed-size adjacency list, one GraphNode
|                |  per DtNode. Provides DFS-based cycle detection
|                |  (white/gray/black coloring) and reverse dependency
|                |  lookup ("who depends on X").
+-------+--------+
        |
        +--------------+--------------+--------------------+
        v              v              v                    v
   text output    DOT export     HTML export          dt_check
   (--tree,       (--format      (--format html,      (--check)
   --deps,        dot)           self-contained,
   --graph)                      click-to-inspect)
```

## Module responsibilities

Each directory under `src/` (mirrored under `include/`) owns exactly
one concern. Nothing in `parser/` knows about dependency types;
nothing in `graph/` knows how to tokenize a `.dts` file.

| Module        | Owns                                                                                          | Does not own                                          |
|----------------|------------------------------------------------------------------------------------------------|---------------------------------------------------------|
| `parser/`      | Character-level tokenization                                                                   | Tree structure, semantics                                |
| `model/`       | `DtNode`/`DtProp` structs, the recursive-descent parser that builds them                         | Phandle resolution, dependency typing                       |
| `resolver/`    | Label/phandle hash maps, sentinel resolution                                                     | Dependency semantics (what a clock *means*)                    |
| `graph/`       | Dependency typing (`dep_engine.c`), adjacency graph + cycle detection (`dep_graph.c`), structural checks (`dt_check.c`) | Output formatting                                                  |
| `exporters/`   | DOT and HTML serialization                                                                       | Any analysis logic                                                    |
| `cli/`         | Argument parsing only                                                                            | Everything else — `main.c` orchestrates, `cli.c` just parses flags      |

## Key design decisions

### Why a hand-written lexer instead of a lexer generator

DTS syntax is small and stable — about a dozen token types. A
hand-written character-at-a-time lexer (`dts_lexer.c`) keeps the build
dependency-free (no `flex`/`bison`) and makes error messages easy to
attach precise line/column information to.

### Why fixed-size arrays instead of dynamic growth

`DT_CELLS_MAX`, `GRAPH_MAX_NODES`, `GRAPH_MAX_EDGES`, `HASH_MAP_CAP`
are all compile-time constants. This was a deliberate simplicity
trade-off for the first working version of every phase — it avoids a
whole class of realloc/resize bugs while the core pipeline was being
proven out. The cost is a practical ceiling on tree size. See
"Roadmap" below for the planned arena-allocator replacement.

### Why the phandle resolver runs in two passes

Pass one (`index_node`) must see every label and every declared
`phandle` before pass two (`resolve_node`) can look any of them up —
a node can be referenced before it's declared in DTS source order
(forward references are legal). Splitting into two full tree walks
avoids needing a deferred-resolution or backpatching mechanism.

### Why `#xxx-cells` lookups happen in the dependency engine, not the resolver

The resolver's only job is "does this phandle number correspond to a
real node." How many argument cells follow a phandle in a property
like `clocks = <&gcc 5>` is a *semantic* question — clock consumers
have `#clock-cells`, DMA consumers have `#dma-cells`, and so on. That
belongs in `dep_engine.c`, which already understands per-property
semantics, not in the resolver.

A bug here during Phase 9 development was instructive: when a target
node had no `#xxx-cells` property at all, the engine fell back to
`0` argument cells and kept scanning — which meant an argument value
could be reinterpreted as a second phandle. The fix was to stop
scanning the rest of that property entirely once a target with a
missing `#xxx-cells` is found, and let `dt_check.c` report the
missing property as its own finding rather than silently
misparsing past it.

## Roadmap

Not yet implemented, in rough priority order:

1. **Arena allocator** — replace fixed-size arrays and per-struct
   `calloc`/`free` with a single arena per parsed tree, freed in one
   call. Removes the `GRAPH_MAX_NODES` / `DT_CELLS_MAX` ceilings.
2. **Binary `.dtb` parsing** — currently `.dts` text source only.
   The FDT binary format (magic `0xd00dfeed`) needs a separate
   reader in `parser/` that produces the same `DtNode` tree.
3. **Devicetree overlay (`.dtso`) merging** — apply an overlay onto a
   base tree before running analysis.
4. **dt-schema / YAML binding validation** — cross-check
   `compatible` strings and required properties against the upstream
   Devicetree Schema bindings, rather than the current heuristic
   `#xxx-cells`-based checks.
5. **Unity test coverage** — `tests/unity/` is vendored but no test
   files exist yet for the lexer, parser, resolver, or `dt_check`
   modules.
