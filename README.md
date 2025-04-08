# slack_tools_cpp

cpp analysis slack tools

## Usage

the binary file located in build/slack_tool

```bash
./build/slack_tool ${yml_file_path}
```

the yml file is used to config the compare/analysis flow, the format is like
this:

## yml format

### compare rpts

```yml
mode: compare
rpts:
  leda0: {path: "rpt/leda_rpts/z80_core_top_N7_LEC_allpath.rpt.gz", type: "leda"}
configs:
  compare_mode: endpoint # [endpoint, startpoint, start_end, full_path]
  analyse_tuples:
    - ["leda0", "leda0"]
  slack_margins: [0.01, 0.03, 0.05, 0.1, 1]
  match_percentages: [0.01, 0.03, 0.1, 0.5, 1]
  output_dir: output
```

Compare result will located in ${output_dir}/${compare_mode}_${match_paths}.csv

### cell in def check

```yml
mode: cell in def
rpts:
  invs0: {path: "rpt/invs_rpts/z80_core_top_N7_LEC_preCTS_popt.tarpt.gz", type: "invs", def: "rpt/defs/z80_core_top_N7_LEC_placeOpt.def"}
configs:
  output_dir: output
  analyse_tuples:
    - [invs0]
```

### arc analyse

```yml
mode: arc analyse
rpts:
  invs0:
    path: rpt/invs_rpts/axi4lscope.simple.gz
    type: invs
  leda:
    path: rpt/leda_rpts/z80_core_top_N7_LEC_allpath.rpt.gz
    type: leda
configs:
  output_dir: output
  analyse_tuples:
    - ["leda", "leda"]
  delay_filter: x >= 0 # filter for delay
```

### path analyse

```yml
mode: path analyse
rpts:
  invs0:
    path: rpt/invs_rpts/axi4lscope.simple.gz
    type: invs
  leda:
    path: rpt/leda_rpts/z80_core_top_N7_LEC_allpath.rpt.gz
    type: leda
configs:
  output_dir: output
  enable_super_arc: True
  enable_ignore_filter: True
  analyse_tuples:
    - - leda
      - leda
```

## explain of config
### Common Attributes Across YAML Modes

1. **Global Structure**:
   - Each YAML file contains a `mode` key to define the type of analysis or comparison.
   - A `rpts` section is used to specify report paths and their types.
   - A `configs` section defines configuration parameters for the specified mode.

2. **Shared Configurations**:
   - **`output_dir`**: Specifies where the output will be stored. This attribute is present in all modes (`compare`, `cell in def`, `arc analyse`, `path analyse`).
   - **`analyse_tuples`**: Defines pairs of reports to analyze. It appears in multiple modes but with varying structures (e.g., single-element lists for `cell in def`, or paired lists for other modes).

3. **Report Definitions**:
   - The `rpts` section consistently uses keys like `path` to specify file locations and `type` to indicate the report format (e.g., `"leda"`, `"invs"`, `"leda_endpoint"`). Additional keys such as `def` appear in specific contexts (like `cell in def` mode).

4. **Mode-Specific Attributes**:
   - **Compare Mode**: Includes unique attributes like `compare_mode`, `slack_margins`, and `match_percentages`.
   - **Arc Analyse Mode**: Introduces a `delay_filter` expression.
   - **Path Analyse Mode**: Features boolean flags like `enable_super_arc` and `enable_ignore_filter`.

### Summary of Shared Attributes

- **`mode`**: Always present to specify operation type.
- **`rpts`**:
  - **`path`**: File path to the report.
  - **`type`**: Type of report (e.g., `"leda"`, `"invs"`).
- **`configs`**:
  - **`output_dir`**: Directory for storing outputs.
  - **`analyse_tuples`**: Pairs or singles of reports to analyze.
