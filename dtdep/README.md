# dtdep — Device Tree Dependency Analyzer

A static analysis tool, written in C, that parses Linux device trees
(`.dts` source files) and builds a dependency graph between hardware
components — clocks, interrupts, DMA channels, resets, GPIOs, and bus
hierarchy. It surfaces structural issues that cause Linux kernel driver
probe failures before you ever boot real hardware.

```
✓ Parsed successfully
  Nodes       : 7
  Properties  : 23
  Tree depth  : 3
  Dependencies: 4

Analysis (1 error, 4 warnings):
  ✗ uart@2000   has 'interrupts' but no 'interrupt-parent' in its ancestry
  ⚠ uart@2000   references 'dmas' (DMA) but target 'dma@700' has no '#dma-cells'
  ⚠ uart@2000   depends on 'reset@600' via 'resets' but that node is disabled
  ⚠ spare@999   declares phandle <0x9> but no other node references it
```

## Why

Device tree bugs are notoriously silent. A missing `interrupt-parent`,
a forgotten `#clock-cells`, or a phandle pointing at a `disabled` node
won't fail at compile time — they fail at runtime, inside
`devm_clk_get()` or `irq_of_parse_and_map()`, often with a cryptic
`-ENOENT` and no indication of which DTS property caused it. `dtdep`
catches these issues statically, before the board ever boots.

## Build

Requires a C11 compiler and GNU Make. No external dependencies for
parsing or analysis; `dot` (Graphviz) is only needed if you choose to
render `--format dot` output to an image.

```bash
make
```

Produces a single binary: `./dtdep`.

## Usage

```
dtdep parse <file.dts> [options]
```

| Flag                 | Description                                            |
|----------------------|----------------------------------------------------------|
| `--tree`             | Print the full parsed node tree                          |
| `--deps`             | Print the flat dependency list                            |
| `--graph`            | Print the adjacency-list graph                             |
| `--check-cycles`     | Run DFS-based dependency cycle detection                    |
| `--depends-on LABEL` | List every node that depends on the node with `LABEL`        |
| `--check`            | Run all structural analysis checks (see below)                |
| `--format dot`       | Emit a Graphviz DOT graph to stdout                             |
| `--format html`      | Emit a self-contained interactive HTML viewer                    |
| `--quiet`            | Suppress the summary banner                                       |
| `--help`             | Show usage                                                          |

### Examples

```bash
# Quick structural summary
./dtdep parse board.dts

# Full driver-breaking issue report
./dtdep parse board.dts --check

# Render an interactive dependency graph you can open in any browser
./dtdep parse board.dts --format html > board.html

# Render a Graphviz image
./dtdep parse board.dts --format dot | dot -Tpng -o board.png

# "If this clock dies, what breaks?"
./dtdep parse board.dts --depends-on gcc
```

## What it checks

`--check` runs seven structural analyses against the parsed tree:

1. **Missing `interrupt-parent`** — a node has `interrupts` but no
   `interrupt-parent` anywhere in its ancestry chain. IRQ routing is
   undefined; `irq_of_parse_and_map()` will fail.
2. **Cell-count validation** — a phandle reference (`clocks`, `dmas`,
   `resets`, `gpios`, `phys`) points at a node missing the matching
   `#xxx-cells` property, so the argument count can't be verified.
3. **Unresolved phandles** — a `&label` reference doesn't resolve to
   any node in the tree. The driver will read a garbage cell value.
4. **Disabled-but-referenced** — a node depends on another node that
   has `status = "disabled"`. The dependency will never be satisfied.
5. **Orphaned phandles** — a node declares a `phandle` that no other
   node in the tree ever references. Possibly dead code.
6. **Dependency cycles** — DFS-based cycle detection across the full
   dependency graph. A cycle causes infinite recursion or deadlock
   during driver probe.
7. **`reg` without cell-size properties** — a node has `reg` but its
   parent is missing `#address-cells` or `#size-cells`, so the
   register address/size can't be parsed with the correct width.

## Architecture

See [`docs/architecture.md`](docs/architecture.md) for the full
pipeline breakdown (lexer → parser → resolver → dependency engine →
graph → exporters) and module boundaries.

```
.dts file
    |
    v
+---------+    +---------+    +----------+    +---------------+    +-------+
|  Lexer  | -> | Parser  | -> | Resolver | -> | Dependency    | -> | Graph |
| (tokens)|    | (tree)  |    |(phandles)|    | engine        |    |       |
+---------+    +---------+    +----------+    | (typed edges) |    +---+---+
                                               +---------------+        |
                                  +----------------------+--------------+
                                  v                      v              v
                             text / --check         DOT export    HTML export
```

## Project layout

```
dtdep/
├── include/             public headers, mirrors src/ layout
│   ├── cli/
│   ├── exporters/
│   ├── graph/
│   ├── model/
│   ├── parser/
│   └── resolver/
├── src/                 implementation
│   ├── cli/             command-line argument parsing
│   ├── exporters/        DOT and HTML output generators
│   ├── graph/            dependency engine, graph, structural checks
│   ├── model/            DtNode / DtProp / DtTree, recursive-descent parser
│   ├── parser/           DTS tokenizer (lexer)
│   ├── resolver/         phandle/label hash map resolution
│   └── main.c            CLI entry point / pipeline orchestration
├── examples/             sample .dts files used in development
├── tests/                Unity test framework + unit tests
└── docs/                 architecture and design notes
```

## Limitations

This is a `.dts` (text source) analyzer. Binary `.dtb` support,
overlay (`.dtso`) merging, and full Devicetree Schema (`dt-schema`,
YAML binding) validation are not yet implemented — see
`docs/architecture.md` for the roadmap. Fixed-size internal buffers
(`DT_NAME_MAX`, `GRAPH_MAX_NODES`, etc.) impose practical limits on
very large trees; see the header files in `include/model/` and
`include/graph/` for current bounds.

## License

See [`LICENSE`](LICENSE).
