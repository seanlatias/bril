import sys
import json
from collections import namedtuple

from form_blocks import form_blocks
import cfg
from util import var_args

# A single dataflow analysis consists of these part:
# - forward: True for forward, False for backward.
# - init: An initial value (bottom or top of the latice).
# - merge: Take a list of values and produce a single value.
# - transfer: The transfer function.
Analysis = namedtuple('Analysis', ['forward', 'init', 'merge', 'transfer'])


def union(sets):
    out = set()
    for s in sets:
        out.update(s)
    return out


def df_worklist(blocks, analysis):
    """The worklist algorithm for iterating a data flow analysis to a
    fixed point.
    """
    preds, succs = cfg.edges(blocks)

    # Switch between directions.
    if analysis.forward:
        first_block = list(blocks.keys())[0]  # Entry.
        in_edges = preds
        out_edges = succs
    else:
        first_block = list(blocks.keys())[-1]  # Exit.
        in_edges = succs
        out_edges = preds

    # Initialize.
    in_ = {node: set() for node in blocks}
    out = {node: set() for node in blocks}
    back = {node: set() for node in blocks}

    # Iterate.
    worklist = list(blocks.keys())
    while worklist:
        node = worklist.pop(0)

        # ancestor accumulation
        for k in in_edges[node]:
          in_[node].update(in_[k])
        for k in in_edges[node]:
          in_[node].add(k)

        origin = out[node]
        for k in out_edges[node]:
          out[node].update(out[k])
        for k in out_edges[node]:
          out[node].add(k)

        # record and remove the back edge  
        overlap = out[node] & in_[node]
        for item in overlap:
          back[item].add(node)
          if item in back[item]:
            back[item].remove(item)
        
        if origin != out[node]:
            out[node] = outval
            worklist += in_edges[node]

    if analysis.forward:
        return in_, out, back
    else:
        return out, in_, back


def fmt(val):
    """Guess a good way to format a data flow value. (Works for sets and
    dicts, at least.)
    """
    if isinstance(val, set):
        if val:
            return ', '.join(v for v in sorted(val))
        else:
            return '∅'
    elif isinstance(val, dict):
        if val:
            return ', '.join('{}: {}'.format(k, v)
                             for k, v in sorted(val.items()))
        else:
            return '∅'
    else:
        return str(val)


def run_df(bril, analysis):
    for func in bril['functions']:
        # Form the CFG.
        blocks = cfg.block_map(form_blocks(func['instrs']))
        cfg.add_terminators(blocks)

        in_, out, back = df_worklist(blocks, analysis)
        for block in blocks:
            print('{}:'.format(block))
            print('  ancestors: ', fmt(in_[block]))
            print('  descendants:', fmt(out[block]))
            print('  back:', fmt(back[block]))


ANALYSES = {
    # Analysis for finding backedge: accumulates ancestor for each block,
    # return intersection between ancestors and predecessors
    'back': Analysis(
        True,
        init=set(),
        merge=union,
        transfer=lambda block, in_: in_.union(gen(block)),
    ),
    
}

if __name__ == '__main__':
    bril = json.load(sys.stdin)
    run_df(bril, ANALYSES[sys.argv[1]])
