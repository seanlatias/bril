from __future__ import division
from __future__ import print_function

import sys
import json
import copy

from form_blocks import form_blocks
from dom import get_dom
from df import df_worklist, ANALYSES
from collections import OrderedDict
from util import flatten
import cfg


# add blocks to a loop given its entry and exit
# does what step 2 describes in the "find_loops" function
def add_blocks_to_loop(entry, exit, preds, dom):
  """
  To do this, we search backwards from the exit so that all blocks on the path can reach the exit. 
  We have to make sure that the nodes are dominated by the entry. 
  """

  # initialize
  l = set([entry, exit])
  working_set = set(preds[exit])

  # run until convergence
  while len(working_set) > 0:
    # initialize the working set for the next round
    new_working_set = set()
    # go through the current working set
    for b in working_set:
      # check if the block is dominated by entry and not in the loop yet
      if (entry in dom[b]) and (not b in l):
        # if so, add the block to the loop
        l.add(b)
        # then propagate, get the predecessors of this block
        pred_list = preds[b]
        # throw everything into the working set, we'll filter things out in the next round
        for p in pred_list:
          # duplicates are handled by the set
          new_working_set.add(p)
    working_set = new_working_set

  return {'entry': entry, 'exit': exit, 'nodes': l}

# check if a loop is our interesting type
def check_loop(l, succs):
  """
  Notice that we are only interested in loops whose tripcounts can be statically computed,
  because we only do full unrolling. So any loop with irregular structures will not be included. 
  By regular, we refer to loops whose control flow can only get out of the loop from either entry or
  exit. 
  """

  for b in l['nodes']:
    if not ((b == l['entry']) or (b == l['exit'])):
      for s in succs[b]:
        if not s in l:
          return False
  
  exit_from_entry = False
  count_entry = 0
  for s in succs[l['entry']]:
    if not s in l['nodes']:
      exit_from_entry = True
      count_entry += 1

  exit_from_exit = False
  count_exit = 0
  for s in succs[l['exit']]:
    if not s in l['nodes']:
      exit_from_exit = True
      count_exit += 1

  if exit_from_exit:
    l['exit_from_exit'] = True
  elif exit_from_entry:
    l['exit_from_exit'] = False

  return not(exit_from_entry and exit_from_exit) or (count_entry != 1) or (count_exit != 1)

# find loops
# algorithm from ECE5775 fall 2018 slides: Control Flow Graph
# credit to Prof. Zhiru Zhang
def find_loops(entry, preds, succs, dom):
  """
  This function finds proper loops for unrolling. We adopt the loop identification algorithm 
  described in Prof. Zhang's slides, which does the following:
    1. Find a back-edge. An edge A-->B is a back-edge if B dominates A. Now we have identified the
       entry (B) and exit (A) of the loop. 
    2. Add nodes to the loop. If a node is dominated by A and can reach B through nodes dominated
       by A, add it to the loop. 
  """

  # we use a dictionary to store loops
  loops = {}
  name_counter = 0
  # iterate through all blocks, get their successors
  for b, succ_list in succs.items():
    # iterate through successors
    for s in succ_list:
      # if b's successor s dominates b, we've found a back-edge!
      if s in dom[b]:
        # now we have a new loop
        new_loop = add_blocks_to_loop(s, b, preds, dom)
        # check if the loop is interesting or not
        if check_loop(new_loop, succs):
          new_loop['name'] = 'L' + str(name_counter)
          loops['L' + str(name_counter)] = new_loop
          name_counter += 1

  return loops

# use the subset operator to check whether a loop contains subloops
def filter_inner_most_loops(loops):

  remove_list = []
  for name1, l1 in loops:
    for name2, l2 in loops:
      if not name1 == name2:
        if l1['nodes'].issuperset(l2['nodes']):
          remove_list.append(l1)
          break

  for l in remove_list:
    del loops[l]

  return loops

# handle trivial cases when computing loop bound
def handle_trivial_case(bounds, op, in_entry):

  tripcount = None
  if bounds[0] < bounds[1]:
    pass # infinite loop
  # if condition never satisfied, if the loop guard is in the exit the loop will be executed once
  elif in_entry:
    tripcount = 0
  else:
    tripcount = 1

  return tripcount

# check if the indvar update instruction is as we expected
def check_inst(inst):
  if not inst['op'] in ['add', 'sub']:
    return False
  if not inst['dest'] in inst['args']:
    return False
  return True

# actually compute the tripcount
def compute_tripcount(op, start, end, step, negate):

  actual_op = op

  if step == 0:
    return None

  # negate the operand if necessary
  # now everything is indvar <op> bound
  if negate:
    if actual_op == 'gt':
      actual_op = 'lt'
    elif actual_op == 'lt':
      actual_op = 'gt'

  if actual_op == 'lt':
    if start >= end: 
      tripcount = 0
    else:
      tripcount = (end - start + step - 1) // step # actually a ceiling
      # nevative values refer to an infinite loop
  elif actual_op == 'gt':
    if start <= end:
      tripcount = 0
    else:
      tripcount = (start - end - step - 1) // (-step)
      # same here

  return tripcount

# find out-going edge
def find_outgoing_edge(loop, succs):
  entry = loop['entry']
  exit = loop['exit']
  nodes = loop['nodes']

  target = None
  source = None
  if loop['exit_from_exit']:
    s = exit
  else:
    s = entry

  outgoing_targets = succs[s]
  for b in outgoing_targets:
    if not b in nodes:
      target = b
      source = s
      break
  assert(target is not None)
  assert(source is not None)

  return target, source

# find the instruction that computes the condition
def find_cond_inst(blocks, loop_nodes, source, br, rds):
  cond = br['args'][0]
  # count the number of reaching definitions of the condition from inside the loop
  # this actually handles the case where the reaching definition is from the source block!
  rd_cnt = 0
  cond_update_inst = None
  for rd in rds:
    if (cond == rd[0]) and (rd[1] in loop_nodes):
      rd_cnt += 1
      cond_update_inst = rd
  # if there is more than one reaching definitions of the condition, give up
  if rd_cnt != 1:
    return None, None

  # actualy retrieve that instruction
  actual_cond_update_inst = None
  for inst in reversed(blocks[cond_update_inst[1]]):
    if 'dest' in inst:
      if inst['dest'] == cond_update_inst[0]:
        actual_cond_update_inst = inst
        break

  # only supporting 'lt' for now
  assert(actual_cond_update_inst['op'] == 'lt')

  return actual_cond_update_inst, cond_update_inst[1]

# get the initial value of induction variable
def get_init_val(nodes, preds, out_cp, indvar):
  init_val = set()
  for b in preds:
    if not b in nodes:
      cp_res = out_cp[b]
      if cp_res[indvar] != '?':
        init_val.add(int(cp_res[indvar]))
  print('Init val is: ', init_val)
  # if we can not locate the only initial value, give up
  if len(init_val) != 1:
    return None

  return init_val.pop()

# compute the tripcount of a loop, return None if it can not be determined
def get_tripcount(loop, preds, succs, in_cp, out_cp, in_rd, out_rd, blocks):

  """
  When we get to this function, the loop should have one and only one edge going out of the loop,
  and the edge must reside in either the entry or the exit block. 
    1. We locate the branch associated with that edge and the condition variable. 
    2. We locate the instruction that generates the condition variable. Notice that we only support
       simple conditions here, i.e. the condition should be generated by a comparison. We further
       simplify by only supporting 'lt'. 
    3. From that instruction, we try to figure out the loop bound. We assume that in the two
       arguments of the condition, one should be the loop bound and the other one should be the
       induction variable. The retrieval of the loop bound completely depends on the power of the
       constant propagation pass. 
    4. After getting the induction variable, we get its initial value by looking at its value right
       before entering the loop. 
    5. We go through all blocks in the loop and find out where the loop induction variable is
       updated. We assume it can only be updated once in the loop, and we only support addition and
       subtraction for now. With all the information retrieved above we can compute the tripcount. 
  """

  target, source = find_outgoing_edge(loop, succs)
  print('Outgoing edge: ', source, '-->', target)

  # find the branch
  br = blocks[source][-1]
  assert(br['op'] == 'br')
  # locate the instruction that computes the condition, together with the block containing it
  cond_inst, cond_inst_block = find_cond_inst(blocks, 
                                              loop['nodes'], 
                                              source, 
                                              br, 
                                              in_rd[source])
  if not cond_inst:
    return None
  print('Condition inst: ', cond_inst, ' in block ', cond_inst_block)

  # get the loop bound
  cp_res = in_cp[cond_inst_block]
  bounds = [None, None]
  for i in range(2):
    arg = cond_inst['args'][i]
    if arg in cp_res:
      if cp_res[arg] != '?':
        bounds[i] = int(cp_res[arg])

  # if both are non-constant, give up
  if (bounds[0] is None) and (bounds[1] is None):
    return None
  # if one of them is constant, then that's the bound
  elif bounds[0] is None:
    bound = bounds[1]
    indvar = cond_inst['args'][0]
    negate_op = False
  elif bounds[1] is None:
    bound = bounds[0]
    indvar = cond_inst['args'][1]
    negate_op = True
  # if both are constant, tripcount can be computed directly
  else:
    return handle_trivial_case(bounds, cond_inst['op'], source == entry)
  print('Induction variable is: ', indvar, ', bound is: ', bound)
  # now we have the loop induction variable, find its initial value
  # initial value is the possible value of this variable when entering the loop
  # thus we check all outgoing sets of constant propagation that comes into the loop entry
  init_val = get_init_val(loop['nodes'], preds[loop['entry']], out_cp, indvar)
  if init_val is None:
    return None

  # find instructions that update the induction variable
  indvar_updates = []
  update_block = None
  for b in loop['nodes']:
    for inst in blocks[b]:
      if 'dest' in inst:
        if inst['dest'] == indvar:
          indvar_updates.append(inst)
          update_block = b
    
  # give up if there is more than one
  if len(indvar_updates) != 1:
    return None
  
  indvar_update_inst = indvar_updates[0]
  print('Induction update instruction is: ', indvar_update_inst)

  # if the instruction is not as we expected, give up
  if not check_inst(indvar_update_inst):
    return None

  # get the step size
  step = None
  for var in indvar_update_inst['args']:
    if var != indvar:
      cp_res = in_cp[update_block]
      if var in cp_res:
        if cp_res[var] != '?':
          step = cp_res[var]
          break

  # if we can not retrieve the step size, give up
  if step is None:
    return None

  # we can finally compute the tripcount... 
  if indvar_update_inst['op'] == 'sub':
    step = -step

  tripcount = compute_tripcount(cond_inst['op'], init_val, bound, step, negate_op)
  return tripcount

# simple condition to check if we unroll the loop or not
def check_unroll(loop, blocks, tripcount):
  if tripcount is None:
    return False
  elif tripcount < 0:
    return False
  else:
    total_insts = 0
    for b in loop['nodes']:
      total_insts += (len(blocks[b]) - 1)
    return (tripcount * total_insts < 1024)

# actually unroll the loops
def unroll(blocks, preds, succs, loop, tripcount):

  if not check_unroll(loop, blocks, tripcount):
    return blocks

  print('Original blocks: ', blocks)
  # add all basic blocks that are not in the loop
  other_blocks = []
  for b in blocks:
    if not b in loop['nodes']:
      other_blocks.append(b)

  new_blocks = OrderedDict()
  for b in other_blocks:
    new_blocks[b] = copy.deepcopy(blocks[b])
  print('New blocks: ', new_blocks.keys())
  
  # change the targets of other blocks that might jump to our loop
  # the only possible case here is that the block will jump to the entry of the loop
  entry = loop['entry']
  exit = loop['exit']
  for block_name, b in new_blocks.items():
    if '_' in block_name:
      raw_block_name = (block_name.rsplit('_', 1))[1]
    else:
      raw_block_name = block_name
    succ = succs[raw_block_name]
    if entry in succ:
      term_inst = b[-1]
      if term_inst['op'] == 'jmp':
        term_inst['args'][0] = loop['name'] + '_0_' + blocks[entry][0]['label']
  
  # start to duplicate the blocks in the loop
  for i in range(tripcount):
    for b in loop['nodes']:
      new_block = copy.deepcopy(blocks[b])
      label = new_block[0]['label']
      new_label = loop['name'] + '_' + str(i) + '_' + label
      new_block[0]['label'] = new_label
      term_inst = new_block[-1]
      # we don't need to change the targets when jumping to out of the loop
      # when jumping to inside the loop, simply change the name
      # exception is the exit, when we handle the backedge we need to do something
      if term_inst['op'] == 'jmp':
        target = term_inst['args'][0]
        if target == entry:
          term_inst['args'][0] = loop['name'] + '_' + str(i+1) + '_' + entry
        elif target in loop['nodes']:
          term_inst['args'][0] = loop['name'] + '_' + str(i) + '_' + target
      elif term_inst['op'] == 'br':
        print(term_inst)
        for j in range(1, len(term_inst['args'])):
          target = term_inst['args'][j]
          if target == entry:
            term_inst['args'][j] = loop['name'] + '_' + str(i+1) + '_' + entry
          elif target in loop['nodes']:
            term_inst['args'][j] = loop['name'] + '_' + str(i) + '_' + target
      new_blocks[new_label] = new_block

  # need to add an extra entry block if the loop exits from the entry
  if not loop['exit_from_exit']:
    new_block = copy.deepcopy(blocks[entry])
    label = new_block[0]['label']
    new_label = loop['name'] + '_' + str(tripcount) + '_' + label
    new_block[0]['label'] = new_label
    # replace the branch in the entry node with a jump
    br = new_block[-1]
    if br['op'] == 'br':
      br['op'] = 'jmp'
      # assume we jump back to the loop if the condition is true
      br['args'] = [br['args'][2]]
    new_blocks[new_label] = new_block

  return new_blocks

# top-level function for loop unrolling
def unroll_loops(bril):

  # this pass operates on each function
  for func in bril['functions']:
    print('Unrolling loops in function ' + func['name'] + '... ')
    # use utilities to form basic blocks
    blocks = cfg.block_map(form_blocks(func['instrs']))
    cfg.add_terminators(blocks)
    entry_block = list(blocks.keys())[0]
    # get all predecessors, successors of the basic blocks
    preds, succs = cfg.edges(blocks)
    print('Preds: ', preds)
    print('Succs: ', succs)

    # compute the dominators
    dom = get_dom(succs, entry_block)
    print('Dom: ', dom)
    
    # now we can find loops
    loops = find_loops(entry_block, preds, succs, dom)
    print('All loops: ')
    print(loops)
    
    # don't unroll loops with subloops, remove them from the dictionary
    interesting_loops = filter_inner_most_loops(loops)
    print('Interesting loops: ')
    print(interesting_loops)
   
    # run constant propagation, try to determine the loop bound and step
    in_consts, out_consts = df_worklist(blocks, ANALYSES['cprop'])
    print('Constant propagation results: ')
    print(in_consts)
    print(out_consts)
    # run reaching definition to help us find the induction variable
    in_rd, out_rd = df_worklist(blocks, ANALYSES['rd'])
    print('Reaching definition results: ')
    print(in_rd)
    print(out_rd)

    # go through all interesting loops
    for _, l in loops.items():
      print('Unrolling loop ', l['name'])
      # compute tripcount
      tripcount = get_tripcount(l, preds, succs, in_consts, out_consts, in_rd, out_rd, blocks)
      print('Tripcount is: ', tripcount)
      # replace
      blocks = unroll(blocks, preds, succs, l, tripcount)

    # replace the instructions in the function
    all_insts = []
    for _, insts in blocks.items():
      all_insts += insts
    func['instrs'] = all_insts


if __name__ == '__main__':
    bril = json.load(sys.stdin)
    unroll_loops(bril)
    with open(sys.argv[1], 'w') as f:
      json.dump(bril, f)
