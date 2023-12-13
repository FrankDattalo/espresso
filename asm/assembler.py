import sys
import pprint
import ctypes

# OpCode structure -
# [Operation - 8 bits, Arg - 24 bits]
OP_BITS          = 0b1111_1111_0000_0000_0000_0000_0000_0000
ARG1_BITS        = 0b0000_0000_1111_1111_0000_0000_0000_0000
ARG2_BITS        = 0b0000_0000_0000_0000_1111_1111_0000_0000
ARG3_BITS        = 0b0000_0000_0000_0000_0000_0000_1111_1111
LARGE_ARG_BITS   = 0b0000_0000_0000_0000_1111_1111_1111_1111
OP_LOAD_CONSTANT = 0b0000_0000_0000_0000_0000_0000_0000_0000
OP_LOAD_GLOBAL   = 0b0000_0001_0000_0000_0000_0000_0000_0000
OP_INVOKE        = 0b0000_0010_0000_0000_0000_0000_0000_0000
OP_RETURN        = 0b0000_0011_0000_0000_0000_0000_0000_0000
OP_COPY          = 0b0000_0100_0000_0000_0000_0000_0000_0000
OP_EQUAL         = 0b0000_0101_0000_0000_0000_0000_0000_0000
OP_LT            = 0b0000_0110_0000_0000_0000_0000_0000_0000
OP_LTE           = 0b0000_0111_0000_0000_0000_0000_0000_0000
OP_GT            = 0b0000_1000_0000_0000_0000_0000_0000_0000
OP_GTE           = 0b0000_1001_0000_0000_0000_0000_0000_0000
OP_ADD           = 0b0000_1010_0000_0000_0000_0000_0000_0000
OP_SUB           = 0b0000_1011_0000_0000_0000_0000_0000_0000
OP_MULT          = 0b0000_1100_0000_0000_0000_0000_0000_0000
OP_DIV           = 0b0000_1101_0000_0000_0000_0000_0000_0000
OP_NOOP          = 0b0000_1110_0000_0000_0000_0000_0000_0000
OP_JUMPF         = 0b0000_1111_0000_0000_0000_0000_0000_0000
OP_JUMP          = 0b0001_0000_0000_0000_0000_0000_0000_0000
OP_STORE_G       = 0b0001_0001_0000_0000_0000_0000_0000_0000
OP_NOT           = 0b0001_0010_0000_0000_0000_0000_0000_0000
OP_MAPSET        = 0b0001_0011_0000_0000_0000_0000_0000_0000
OP_NEWMAP        = 0b0001_0100_0000_0000_0000_0000_0000_0000

# Constant structure -
# [Tag - 8 bits, Variable length depending on tag ...]
CONST_BITS   = 0b11111111
CONST_NIL    = 0b00000000, 'nil'
CONST_INT    = 0b00000001, 'int'
CONST_REAL   = 0b00000010, 'real'
CONST_STRING = 0b00000011, 'string'
CONST_BOOL   = 0b00000100, 'boolean'
CONST_FUNC   = 0b00000101, 'function'

def main():
    source = [ line.strip() for line in sys.stdin ]
    source = [ line for line in source if not line.strip().startswith('//') ]
    source = ' '.join(source).strip()
    split = source.split(' ')
    source = []

    def is_mid_string(item):
        return len(source) > 0 \
        and source[-1].startswith('"') \
        and ((not source[-1].endswith('"')) or len(source[-1]) == 1)

    for item in split:
        if is_mid_string(item):
            source[-1] = source[-1] + " " + item
        else:
            source.append(item)

    source = [ item.strip() for item in source ]
    source = [ item for item in source if len(item) > 0 ]

    def new_context():
        return {"labels": {}, "arity": 0, "locals": 0, "opcodes": [], "constants": [], "max_register": 0}

    assembler = {
        "index": 0,
        "source": source,
        "output": bytearray(),
        "contexts": [new_context()]
    }

    def emit(opcode):
        assembler['contexts'][-1]['opcodes'].append(opcode)

    def constant(val):
        assembler['contexts'][-1]['constants'].append(val)

    def writeU8(v):
        assembler['output'] += int.to_bytes(v, byteorder='big', signed=False, length=1)

    def writeU16(v):
        assembler['output'] += int.to_bytes(v, byteorder='big', signed=False, length=2)

    def writeU32(v):
        assembler['output'] += int.to_bytes(v, byteorder='big', signed=False, length=4)

    def writeU64(v):
        assembler['output'] += int.to_bytes(v, byteorder='big', signed=False, length=8)

    def writeI64(v):
        assembler['output'] += int.to_bytes(v, byteorder='big', signed=True, length=8)

    def writeF64(v):
        import struct
        data = struct.pack('>d', v)
        data = [ b for b in data ]
        data = int.from_bytes(bytearray(data), byteorder='big', signed=False)
        writeU64(data)

    def next():
        curr = assembler['source'][assembler['index']]
        assembler['index'] += 1
        return curr

    def next_register():
        reg = next()
        if not reg.startswith('R'):
            raise Exception(f'Invalid register: {reg}')
        res = int(reg[len('R'):])
        if res < 0:
            raise Exception(f'Invalid register: {reg}')

        assembler['contexts'][-1]['max_register'] = max(
            res, assembler['contexts'][-1]['max_register'])

        return res

    def next_int():
        n = next()
        if n == '#C':
            return len(assembler['contexts'][-1]['constants'])
        if n == '#R':
            return assembler['contexts'][-1]['max_register'] + 1
        return int(n)

    def next_label():
        n = next()
        if not n.startswith('@'):
            raise Exception(f'Invalid label: {n}')
        return n

    def arity():
        assembler['contexts'][-1]['arity'] = next_int()

    def set_locals():
        assembler['contexts'][-1]['locals'] = next_int()

    def Arg1(v):
        return (v << 16) & ARG1_BITS

    def Arg2(v):
        return (v << 8) & ARG2_BITS

    def Arg3(v):
        return v & ARG3_BITS

    def LargeArg(v):
        return v & LARGE_ARG_BITS

    def load_constant():
        dest_reg = next_register()
        const_number = next_int()
        emit(OP_LOAD_CONSTANT | Arg1(dest_reg) | LargeArg(const_number))

    def load_global():
        dest_reg = next_register()
        source_reg = next_register()
        emit(OP_LOAD_GLOBAL | Arg1(dest_reg) | Arg2(source_reg))

    def invoke():
        base_reg = next_register()
        argument_count = next_int()
        emit(OP_INVOKE | Arg1(base_reg) | Arg2(argument_count))

    def op_newmap():
        base_reg = next_register()
        emit(OP_NEWMAP | Arg1(base_reg))

    def op_return():
        base_reg = next_register()
        emit(OP_RETURN | Arg1(base_reg))

    def copy():
        dest = next_register()
        source = next_register()
        emit(OP_COPY | Arg1(dest) | Arg2(source))

    def equal():
        dest = next_register()
        source1 = next_register()
        source2 = next_register()
        emit(OP_EQUAL | Arg1(dest) | Arg2(source1) | Arg3(source2))

    def mapset():
        dest = next_register()
        source1 = next_register()
        source2 = next_register()
        emit(OP_MAPSET | Arg1(dest) | Arg2(source1) | Arg3(source2))

    def string():
        val = next()
        val = val[1:-1]
        constant((CONST_STRING, val))

    def nil():
        constant((CONST_NIL, None))

    def integer():
        val = next_int()
        constant((CONST_INT, val))

    def op_float():
        val = float(next())
        constant((CONST_REAL, val))

    def function():
        assembler['contexts'].append(new_context())
        assembler['contexts'][-2]['constants'].append((CONST_FUNC, assembler['contexts'][-1]))

    def resolve_label(label):
        return assembler['contexts'][-1]['labels'][label]

    def patch(func, opcode):
        if type(opcode) is int:
            return opcode
        if opcode[0] == 'jumpf':
            _, reg, label = opcode
            location = resolve_label(label)
            return OP_JUMPF | Arg1(reg) | LargeArg(location)
        elif opcode[0] == 'jump':
            _, label = opcode
            location = resolve_label(label)
            return OP_JUMP | LargeArg(location)
        else:
            raise Exception(f'Invalid opcode to patch: {opcode}')

    def patch_function(func):
        func['opcodes'] = [patch(func, opcode) for opcode in func['opcodes']]

    def endfunction():
        func = assembler['contexts'][-1]
        patch_function(func)
        assembler['contexts'].pop()

    def jump_if_false():
        reg = next_register()
        label = next_label()
        emit(('jumpf', reg, label))

    def jump():
        label = next_label()
        emit(('jump', label))

    def label():
        label = next_label()
        location = len(assembler['contexts'][-1]['opcodes'])
        assembler['contexts'][-1]['labels'][label] = location

    def sub():
        dest = next_register()
        source1 = next_register()
        source2 = next_register()
        emit(OP_SUB   | Arg1(dest) | Arg2(source1) | Arg3(source2))

    def add():
        dest = next_register()
        source1 = next_register()
        source2 = next_register()
        emit(OP_ADD   | Arg1(dest) | Arg2(source1) | Arg3(source2))

    def mult():
        dest = next_register()
        source1 = next_register()
        source2 = next_register()
        emit(OP_MULT  | Arg1(dest) | Arg2(source1) | Arg3(source2))

    def store_global():
        key = next_register()
        value = next_register()
        emit(OP_STORE_G | Arg1(key) | Arg2(value))

    def op_not():
        dest = next_register()
        source = next_register()
        emit(OP_NOT | Arg1(dest) | Arg2(source))

    handlers = {
        'arity': arity,
        'locals': set_locals,
        'loadc': load_constant,
        'loadg': load_global,
        'invoke': invoke,
        'string': string,
        'function': function,
        'end': endfunction,
        'return': op_return,
        'copy': copy,
        'nil': nil,
        'integer': integer,
        'equal': equal,
        'jumpf': jump_if_false,
        'jump': jump,
        'label': label,
        'sub': sub,
        'add': add,
        'mult': mult,
        'float': op_float,
        'storeg': store_global,
        'not': op_not,
        'mapset': mapset,
        'newmap': op_newmap,
    }

    while assembler['index'] < len(assembler['source']):
        curr = next()
        if curr not in handlers:
            raise Exception(f'Invalid operation: {curr} at {assembler["index"]} {source}')
        handler = handlers[curr]
        handler()

    def write_constant(const):
        def write_string(string):
            writeU32(len(string))
            for c in string:
                writeU8(ord(c))
        def write_nil(v):
            pass
        def write_map(v):
            pass
        def write_int(v):
            writeI64(v)
        def write_float(v):
            writeF64(v)
        handlers = {
            CONST_STRING: write_string,
            CONST_FUNC: write_function,
            CONST_NIL: write_nil,
            CONST_INT: write_int,
            CONST_REAL: write_float,
        }
        const_type, val = const
        writeU8(const_type[0])
        handler = handlers[const_type]
        handler(val)

    def write_function(func):
        writeU16(func['arity'])
        writeU16(func['locals'])
        writeU16(len(func['opcodes']))
        for opcode in func['opcodes']:
            writeU32(opcode)
        writeU16(len(func['constants']))
        for constant in func['constants']:
            write_constant(constant)

    try:
        patch_function(assembler['contexts'][0])
        write_function(assembler['contexts'][0])

    except Exception as e:
        sys.stderr.write(pprint.pformat(e) + '\n')
        sys.stderr.write(pprint.pformat(assembler) + '\n')
        raise e

    sys.stdout.buffer.write(assembler['output'])

if __name__ == '__main__':
    main()
