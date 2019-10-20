#!/usr/bin/env python3
import json
import sys
import operator
json_str = sys.stdin.read()
obj = json.loads(json_str)
def main_template(body):
    template = """#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
int main(){{
{}
return 0;
}}"""
    return template.format(body)


body = """
    return 0;
"""
main_function = obj['functions'][0]
instrs = main_function['instrs']
variables = dict()

def const(instr):
    if instr["dest"] not in variables:
        variables[instr["dest"]] = instr["type"]
    value = instr["value"]

    #print("!!!!"+str(instr["value"]))
    if instr["type"] == "bool":
        if instr["value"] == False:
            value = 0
        else:
            value = 1
    return "{} = {};".format(instr["dest"], value)

def br(instr):
    return"""if({})
    goto {};
else
    goto {};""".format(instr["args"][0], instr["args"][1], instr["args"][2])

def jmp(instr):
    return "goto {};".format(instr["args"][0])

def bid(instr):
    if instr["dest"] not in variables:
        variables[instr["dest"]] = instr["type"]

    return "{} = {};".format(instr["dest"], instr["args"][0])

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
        variables[instr["dest"]] = instr["type"]
    return "{} = {} {} {};".format(instr["dest"], instr["args"][0], op, instr["args"][1])

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
        variables[instr["dest"]] = instr["type"]
    return "{} = {} {} {};".format(instr["dest"], instr["args"][0], op, instr["args"][1])

def nop(instr):
    return ""

def ret(instr):
    return "return 0;"

def new(instr):
    return instr["type"]["base"] + " " + instr["dest"] + "[" + str(instr["type"]["size"]) + "];"

def set(instr):
    return instr["args"][0] + "[" + instr["args"][1] + "] = " + instr["args"][2] + ";";

def index(instr):
    if instr["dest"] not in variables:
        variables[instr["dest"]] = instr["type"]

    return instr["dest"] + " = " + instr["args"][0] + "[" + instr["args"][1] + "];";

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

body = []
for i in instrs:
    print(i)
    if "op" in i:
        code = opcode[i["op"]](i)
    elif "label" in i:
        code = label(i)
    body.append(code)

type_declr={"int":"int64_t", "bool":"int"};
declr = ["{} {};".format(type_declr[v], k) for k,v in variables.items()]


print(main_template("\n".join(declr+body)))
