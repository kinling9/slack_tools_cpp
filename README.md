# slack_tools_cpp

cpp analysis slack tools

## Usage

the binary file located in build/scr/slack_tool

```bash
./build/scr/slack_tool ${yml_file_path}
```

the yml file is used to config the compare/analysis flow, the format is like
this:

## yml format

### compare rpts

```yml
mode: compare
compare_mode: full_path # [endpoint, startpoint, start_end, full_path]
rpts: 
  - ["rpt/invs_rpts/z80_core_top_N7_LEC_preCTS_place.tarpt.gz", "rpt/invs_rpts/z80_core_top_N7_LEC_preCTS_popt.tarpt.gz"]
  - ["rpt/invs_rpts/axi4lscope.simple.gz", "rpt/invs_rpts/axi4lscope.simple.gz"]
type: ["invs", "invs"] # [invs, leda]
slack_margins: [0.01, 0.03, 0.05, 0.1, 1] # optional
match_percentages: [0.01, 0.03, 0.1, 0.5, 1] # optional
output_dir: output # optional
match_paths: 10 # optional
```

Compare result will located in ${output_dir}/${compare_mode}_${match_paths}.csv

### cell in def check

```yml
mode: cell in def
rpt_defs:
  - ["rpt/invs_rpts/z80_core_top_N7_LEC_preCTS_popt.tarpt.gz", "rpt/defs/z80_core_top_N7_LEC_placeOpt.def"]
rpt_type: invs
output_dir: output
```
