#!/usr/bin/env python3
import json
import sys
import copy
import operator
from datetime import datetime

from cfg import block_map, successors, add_terminators
from form_blocks import form_blocks

json_str = sys.stdin.read()
obj = json.loads(json_str)
variables = dict()

# register var
def const(instr):
    if instr["dest"] not in variables:
        variables[instr["dest"]] = []

    value = eval("(" + instr["type"] + \
            ")(" + str(instr["value"]) + ")")
    if instr["type"] == "bool":
        if instr["value"] == False:
            value = 0
        else:
            value = 1
    time = datetime.timestamp(datetime.now())
    variables[instr["dest"]].append((value, time))

def br(instr):
    cond = variables[instr["args"][0]][-1][0] 
    if cond == 1: return instr["args"][1]
    else: return instr["args"][2]

def jmp(instr):
    return instr["args"][0]

# assign value to var
def bid(instr):
    if instr["dest"] not in variables:
        variables[instr["dest"]] = []
    time = datetime.timestamp(datetime.now())
    value = variables[instr["args"][0]][-1][0]
    variables[instr["dest"]].append((value, time))

def bprint(instr):
    if variables[instr["args"][0]] == "int":
        return 'printf("%" PRId64 "\\n", {});'.format(instr["args"][0])
    if variables[instr["args"][0]] == "bool":
        return 'printf({}?"true\\n":"false\\n");'.format(instr["args"][0])

def arith(instr):
    ops={"add":"+",
    "sub":"-",
    "mul":"*",
    "div":"/",
    "and":"&",
    "or":"|"
    }
    op = ops[instr["op"]]
    if instr["dest"] not in variables:
        variables[instr["dest"]] = []
    left  = variables[instr["args"][0]][-1][0]
    right = variables[instr["args"][1]][-1][0]
    value = eval(str(left) + op + str(right))
    time = datetime.timestamp(datetime.now())
    variables[instr["dest"]].append((value, time)) 

def uniarith(instr):
    ops={"not":"!"}
    op = ops[instr["op"]]
    if instr["dest"] not in variables:
        variables[instr["dest"]] = instr["type"]
    return "{} = {} {};".format(instr["dest"], op, instr["args"][0])

def compare(instr):
    ops = {"eq": "==",
          "lt": "<",
          "gt": ">",
          "le": "<=",
          "ge": ">="}
    op = ops[instr["op"]]
    if instr["dest"] not in variables:
        variables[instr["dest"]] = []
    left  = variables[instr["args"][0]][-1][0]
    right = variables[instr["args"][1]][-1][0]
    value = eval(str(left) + op + str(right))
    if value: value = 1
    else: value = 0
    time = datetime.timestamp(datetime.now())
    variables[instr["dest"]].append((value, time))

def nop(instr):
    pass

def ret(instr):
    pass

# create new arr
def new(instr):
    if instr["dest"] not in variables:
        variables[instr["dest"]] = []
    dtype = instr["type"]["base"]
    size = instr["type"]["size"]
    time = datetime.timestamp(datetime.now())
    variables[instr["dest"]].append(([0] * size, time))

# set arr
def set(instr):
    index = variables[instr["args"][1]][-1][0]
    value = variables[instr["args"][2]][-1][0]
    origin = variables[instr["args"][0]][-1][0]
    new = copy.deepcopy(origin)
    new[index] = value
    time = datetime.timestamp(datetime.now())
    arr = variables[instr["args"][0]].append((new, time))

# get arr value
def index(instr):
    if instr["dest"] not in variables:
        variables[instr["dest"]] = []
    arr = variables[instr["args"][0]][-1][0]
    index = variables[instr["args"][1]][-1][0]
    value = arr[index]
    time = datetime.timestamp(datetime.now())
    variables[instr["dest"]].append((value, time));

opcode = {
"const": const,
"br": br,
"jmp": jmp,
"print": bprint,
"add": arith,
"mul": arith,
"sub": arith,
"div": arith,
"and": arith,
"or": arith,
"not": uniarith,
"eq": compare,
"lt": compare,
"gt": compare,
"le": compare,
"ge": compare,
"nop": nop,
"ret": ret,
"id": bid,
"new": new,
"set": set,
"index": index,
}

def label(instr):
    return "{}:;".format(instr["label"])

# form basic block maps 
funcs = obj['functions']
instrs = funcs[0]['instrs']
blocks = block_map(form_blocks(instrs))
add_terminators(blocks)
succ = {name: successors(block[-1]) for name, block in blocks.items()}

flag = True
block = blocks['b1']
while flag:
  for i in block:
    print(i)
    if "op" in i:
        ret = opcode[i["op"]](i)
        if ret: # jump or br
          print(ret)
          name = ret
          break
        if i["op"] == "ret":
          flag = False; break
    elif "label" in i: pass
  block = blocks[name] 

type_declr={"int":"int64_t", "bool":"int"};
for k,v in variables.items():
    print(k, v)
