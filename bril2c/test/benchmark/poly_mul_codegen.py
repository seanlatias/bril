import random
import argparse

parser = argparse.ArgumentParser(description='Generate Bril program for polynomial multiplication')
parser.add_argument('n', type=int, help='the size of the input polynomials')
parser.add_argument('--no-print', action='store_true', help='do not print the result in the end')
args = parser.parse_args()
n = args.n

a_vars = ["a_{}".format(i) for i in range(n)]
b_vars = ["b_{}".format(i) for i in range(n)]
c_vars = ["c_{}".format(i) for i in range(2*n-1)]

def init_vars(vs, f):
    for v in vs:
        print("{}:int = const {};".format(v, f()))

def mul_symbol(c,a,b):
    for i,ai in enumerate(a):
        for j,bi in enumerate(b):
            print("{}:int = mul {} {};".format("tmp",ai,bi))
            print("{}:int = add {} {};".format(c[i+j],c[i+j],"tmp"))

def print_vars(vs):
    for v in vs:
        print("print {};".format(v))

print("main{")
init_vars(a_vars, lambda :random.randint(0,100))
init_vars(b_vars, lambda :random.randint(0,100))
init_vars(c_vars, lambda :0)
mul_symbol(c_vars, a_vars, b_vars)
if not args.no_print:
    print_vars(c_vars)
print("}")

