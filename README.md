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
  leda0: {path: "rpt/leda_rpts/B005_allpath.rpt.gz", type: "leda"}
  leda1: {path: "rpt/leda_rpts/B005_allpath2.rpt.gz", type: "leda"}
  leda2: {path: "rpt/leda_rpts/SEED_allpath.rpt.gz", type: "leda"}
  leda3: {path: "rpt/leda_rpts/SEED_allpath2.rpt.gz", type: "leda"}
configs:
  compare_mode: endpoint # [endpoint, startpoint, start_end, full_path]
  analyse_tuples:
    - ["leda0", "leda1"]
    - ["leda2", "leda3"]
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
