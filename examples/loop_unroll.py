"""
                                    Loop Unrolling Pass

  This pass does full unrolling on selected loops. Limitations apply. 
    1. Assumptions on the condition: the condition for the loop should be a 'lt' with the loop
       induction variable being the first argument. 
    2. Assumptions on the branch: the loop guard should branch back to the loop when the condition
       is True and jump out of the loop when the condition is False. 
    3. Assumptions on the instruction that updates the induction variable: should be of the form
       i = i + 4, i = i - 5, etc. 
    4. We assume that copy propagation has already been performed on the code.  
"""


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


###################################################################################################
#                                 Functions used to find loops                                    #
###################################################################################################

# add blocks to a loop given its entry and exit
# does what step 2 describes in the "find_loops" function
def add_blocks_to_loop(entry, exit, preds, dominators):
  """
  To do this, we search backwards from the exit so that all blocks on the path can reach the exit. 
  We have to make sure that the nodes are dominated by the entry. 
  """

  loop = [entry, exit]
  working_set = set(preds[exit])

  while len(working_set) > 0:
    new_working_set = set()
    for bb in working_set:
      if (entry in dominators[bb]) and (not bb in loop):
        loop.append(bb)
        pred_list = preds[bb]
        for predecessor  in pred_list:
          new_working_set.add(predecessor)
    working_set = new_working_set

  return {'entry': entry, 'exit': exit, 'nodes': loop}

# check if a loop is our interesting type
def check_loop(loop, succs):
  """
  Notice that we are only interested in loops whose tripcounts can be statically computed,
  because we only do full unrolling. So any loop with irregular structures will not be included. 
  By regular, we refer to loops whose control flow can only get out of the loop from either entry or
  exit. 
  """

  # check for outgoing edges that are not in entry or exit
  for bb in loop['nodes']:
    if not ((bb == loop['entry']) or (bb == loop['exit'])):
      for successor in succs[bb]:
        if not successor in loop:
          return False
  
  # count the number of outgoing edges in entry and exit
  exit_pos = {'entry': False, 'exit': False}
  counts = {'entry': 0, 'exit': 0}
  for bb_key in ['entry', 'exit']:
    bb = loop[bb_key]
    for successor in succs[bb]:
      if not successor in loop['nodes']:
        exit_pos[bb_key] = True
        counts[bb_key] += 1

  # add a flag to indicate the exit position
  loop['exit_from_exit'] = exit_pos['exit']

  # condition to check whether the loop is interesting or not
  is_valid = not(exit_pos['entry'] and exit_pos['exit']) 
  is_valid = is_valid or (counts['entry'] != 1) or (counts['exit'] != 1)
  return is_valid 

# use the subset operator to check whether a loop contains subloops
def filter_inner_most_loops(loops):

  remove_list = []
  for name1, loop1 in loops.items():
    for name2, loop2 in loops.items():
      if not name1 == name2:
        set1 = set(loop1['nodes'])
        set2 = set(loop2['nodes'])
        if set1.issuperset(set2):
          remove_list.append(loop1)
          break

  for l in remove_list:
    del loops[l]

  return loops

# find loops
# algorithm from ECE5775 fall 2018 slides: Control Flow Graph
# credit to Prof. Zhiru Zhang
def find_loops(entry, preds, succs, dominators):
  """
  This function finds proper loops for unrolling. We adopt the loop identification algorithm 
  described in Prof. Zhang's slides, which does the following:
    1. Find a back-edge. An edge A-->B is a back-edge if B dominates A. Now we have identified the
       entry (B) and exit (A) of the loop. 
    2. Add nodes to the loop. If a node is dominated by A and can reach B through nodes dominated
       by A, add it to the loop. 
  """

  loops = {}
  name_counter = 0
  for bb, succ_list in succs.items():
    for successor in succ_list:
      if successor in dominators[bb]:
        new_loop = add_blocks_to_loop(successor, bb, preds, dominators)
        if check_loop(new_loop, succs):
          new_loop['name'] = 'L' + str(name_counter)
          loops['L' + str(name_counter)] = new_loop
          name_counter += 1

  # we only unroll loops that do not have subloops
  loops = filter_inner_most_loops(loops)

  return loops

###################################################################################################
#                            Functions used to compute tripcount                                  #
###################################################################################################

# find out-going edge
def find_outgoing_edge(loop, succs):
  entry = loop['entry']
  exit = loop['exit']
  nodes = loop['nodes']

  target = None
  if loop['exit_from_exit']:
    source = exit
  else:
    source = entry

  outgoing_targets = succs[source]
  for bb in outgoing_targets:
    if not bb in nodes:
      target = bb
      break
  assert(target is not None)

  return target

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

# get induction variable and bound
def get_indvar_and_bound(cond_inst, cp_res):

  bound = None

  # assume the bound is at the second argument
  bound_arg = cond_inst['args'][1]
  if bound_arg in cp_res:
    if cp_res[bound_arg] != '?':
      bound = int(cp_res[bound_arg])

  if bound is not None:
    indvar = cond_inst['args'][0]
  else:
    indvar = None
  
  return indvar, bound

# get the initial value of induction variable
def get_init_val(nodes, preds, out_cp, indvar):
  init_val = set()
  for b in preds:
    if not b in nodes:
      cp_res = out_cp[b]
      if cp_res[indvar] != '?':
        init_val.add(int(cp_res[indvar]))
  # if we can not locate the only initial value, give up
  if len(init_val) != 1:
    return None

  return int(init_val.pop())

# check if the indvar update instruction is as we expected
# should be of the form: i = i + 4
def check_inst(inst):
  if not inst['op'] in ['add', 'sub']:
    return False
  if not inst['dest'] in inst['args']:
    return False
  return True

# get the step size
def get_step_size(loop_nodes, blocks, in_cp, indvar):

  indvar_updates = []
  for bb in loop_nodes:
    for inst in blocks[bb]:
      if 'dest' in inst:
        if inst['dest'] == indvar:
          indvar_updates.append((inst, bb))
    
  # give up if there is more than one
  if len(indvar_updates) != 1:
    return None
  
  indvar_update_inst = indvar_updates[0][0]
  update_block = indvar_updates[0][1]
  print('Induction update instruction is: ', indvar_update_inst, ' in block ', update_block)

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

  # we can finally compute the tripcount... 
  if (indvar_update_inst['op'] == 'sub') and (step is not None):
    step = -step

  return step

# actually compute the tripcount
def compute_tripcount(start, end, step):

  if step == 0:
    return None

  if start >= end: 
    tripcount = 0
  else:
    tripcount = (end - start + step - 1) // step # actually a ceiling
    # nevative values refer to an infinite loop

  return tripcount

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

  target = find_outgoing_edge(loop, succs)
  source = loop['exit'] if loop['exit_from_exit'] else loop['entry']
  print('Outgoing edge: ', source, '-->', target)

  br = blocks[source][-1]
  assert(br['op'] == 'br')
  cond_inst, cond_inst_bb = find_cond_inst(blocks, 
                                           loop['nodes'], 
                                           source, 
                                           br, 
                                           in_rd[source])

  if not cond_inst:
    print('Can not find conditional instruction... ')
    return None
  print('Condition inst: ', cond_inst, ' in block ', cond_inst_bb)

  # get the induction variable and loop bound
  indvar, bound = get_indvar_and_bound(cond_inst, in_cp[cond_inst_bb])
  if not indvar:
    print('Can not find induction variable...')
    return None
  if not bound:
    print('Can not determine loop bound...')
    return None
  print('Induction variable is: ', indvar, ', bound is: ', bound)

  # find the initial value of induction variable
  init_val = get_init_val(loop['nodes'], preds[loop['entry']], out_cp, indvar)
  if init_val is None:
    print('Can not determine initial value of induction variable... ')
    return None
  print('Initial value of induction variable is: ', init_val)

  # find instructions that update the induction variable
  step = get_step_size(loop['nodes'], blocks, in_cp, indvar)
  # if we can not retrieve the step size, give up
  if step is None:
    print('Can not determine step size... ')
    return None
  print('Step size is ', step)

  tripcount = compute_tripcount(init_val, bound, step)
  return tripcount


###################################################################################################
#                           Functions used to actually unroll loops                               #
###################################################################################################

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

  # add all basic blocks that are not in the loop
  other_blocks = []
  for bb in blocks:
    if not bb in loop['nodes']:
      other_blocks.append(bb)

  new_blocks = OrderedDict()
  for bb in other_blocks:
    new_blocks[bb] = copy.deepcopy(blocks[bb])
 
  name_str = '{0}_{1}_{2}'
  entry = loop['entry']
  exit = loop['exit']
  exit_from_exit = loop['exit_from_exit']

  # change the targets of other blocks that might jump to our loop
  # the only possible case here is that the block will jump to the entry of the loop
  for block_name, bb in new_blocks.items():
    # clean up the block names, if we want to do it more rigorously we should use re
    if '_' in block_name:
      raw_block_name = (block_name.rsplit('_', 1))[1]
    else:
      raw_block_name = block_name
    succ_list = succs[raw_block_name]
    if entry in succ_list:
      term_inst = bb[-1]
      if term_inst['op'] == 'jmp':
        term_inst['args'][0] = name_str.format(loop['name'], 0, entry)
      elif term_inst['op'] == 'br':
        for i in range(1, len(term_inst['args'])):
          if term_inst['args'][i] == entry:
            term_inst['args'][i] = name_str.format(loop['name'], 0, entry)
  
  loop_blocks = OrderedDict()
  # start to duplicate the blocks in the loop
  for i in range(tripcount):
    for bb in loop['nodes']:
      new_block = copy.deepcopy(blocks[bb])
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
          term_inst['args'][0] = name_str.format(loop['name'], i+1, entry)
        elif target in loop['nodes']:
          term_inst['args'][0] = name_str.format(loop['name'], i, target)
      elif term_inst['op'] == 'br':
        targets = term_inst['args'][1:]
        in_loop = [t in loop['nodes'] for t in targets]
        # two possible conditions: the first target is in the loop and second one is not
        if in_loop[0] and not in_loop[1]:
          term_inst['op'] = 'jmp'
          if targets[0] == entry:
            term_inst['args'] = [name_str.format(loop['name'], i+1, entry)]
          else:
            term_inst['args'] = [name_str.format(loop['name'], i, targets[0])]
        # or both are in loop
      else:
        for j in range(1, len(term_inst['args'])):
          target = term_inst['args'][j]
          term_inst['args'][j] = name_str.format(loop['name'], i, target)

      loop_blocks[new_label] = new_block

  # need to add an extra entry block if the loop exits from the entry
  if not loop['exit_from_exit']:
    new_block = copy.deepcopy(blocks[entry])
    new_label = name_str.format(loop['name'], tripcount, entry)
    new_block[0]['label'] = new_label
    # replace the branch in the entry node with a jump
    br = new_block[-1]
    if br['op'] == 'br':
      br['op'] = 'jmp'
      # assume we jump back to the loop if the condition is true
      br['args'] = [br['args'][2]]
    loop_blocks[new_label] = new_block
  # otherwise, we need to replace the branch in the last exit block to a jump
  else:
    last_exit_block_name = name_str.format(loop['name'], tripcount-1, exit)
    bb = loop_blocks[last_exit_block_name]
    br = bb[-1]
    assert(br['op'] == 'br')
    bb[-1]['op'] = 'jmp'
    bb[-1]['args'] = [br['args'][2]]

  # (conceptually) merge the basic blocks
  # what we need to do is to remove all the labels of the entry and exit blocks, and all the jumps 
  # at the end of the entry blocks and exit blocks
  for i in range(tripcount+1):
    exit_block_name = name_str.format(loop['name'], i, exit)
    if exit_block_name in loop_blocks:
      # remove the label
      print(loop_blocks[exit_block_name][0])
      del loop_blocks[exit_block_name][0]
      # remove the jump at the end
      if (not loop['exit_from_exit']) or (i < tripcount - 1):
        print(loop_blocks[exit_block_name][-1])
        del loop_blocks[exit_block_name][-1]
    entry_block_name = name_str.format(loop['name'], i, entry)
    if entry_block_name in loop_blocks:
      # remove the label
      if i > 0:
        print(loop_blocks[entry_block_name][0])
        del loop_blocks[entry_block_name][0]
      # remove the jump at the end
      if (loop_blocks[entry_block_name][-1]['op'] == 'jmp') and (i < tripcount):
        print(loop_blocks[entry_block_name][-1])
        del loop_blocks[entry_block_name][-1]
  

  return {**new_blocks, **loop_blocks}

 
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
    
    # run constant propagation, try to determine the loop bound and step
    in_consts, out_consts = df_worklist(blocks, ANALYSES['cprop'])
    # run reaching definition to help us find the induction variable
    in_rd, out_rd = df_worklist(blocks, ANALYSES['rd'])
    print('Done with dataflow analysis: constant propagation and reaching definition. ')

    # now we can find loops
    loops = find_loops(entry_block, preds, succs, dom)
    print('All loops: ')
    print(loops)

    # go through all interesting loops
    for _, l in loops.items():
      print('Unrolling loop ', l['name'])
      print(l['nodes'])
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
