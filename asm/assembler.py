import sys
import pprint
import ctypes

# OpCode structure -
# [Operation - 8 bits, Arg1 - 12 bits, ARG2 - 12 bits]
OP_BITS          = 0b1111_1111_0000_0000_0000_0000_0000_0000
ARG1_BITS        = 0b0000_0000_1111_1111_1111_0000_0000_0000
ARG2_BITS        = 0b0000_0000_0000_0000_0000_1111_1111_1111
OP_LOAD_CONSTANT = 0b0000_0001_0000_0000_0000_0000_0000_0000, 'lc'
OP_LOAD_GLOBAL   = 0b0000_0010_0000_0000_0000_0000_0000_0000, 'gc'
OP_INVOKE        = 0b0000_0011_0000_0000_0000_0000_0000_0000, 'invoke'
OP_RETURN        = 0b0000_0100_0000_0000_0000_0000_0000_0000, 'return'

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
        return {"arity": 0, "locals": 0, "opcodes": [], "constants": []}

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

    def next():
        curr = assembler['source'][assembler['index']]
        assembler['index'] += 1
        return curr

    def next_register():
        reg = next()
        if not reg.startswith('%R'):
            raise Exception(f'Invalid register: {reg}')
        reg = int(reg[len('%R'):])
        return reg

    def next_int():
        return int(next())

    def arity():
        assembler['contexts'][-1]['arity'] = next_int()

    def locals():
        assembler['contexts'][-1]['locals'] = next_int()

    def load_constant():
        dest_reg = next_register()
        const_number = next_int()
        emit((OP_LOAD_CONSTANT, dest_reg, const_number))

    def load_global():
        dest_reg = next_register()
        source_reg = next_register()
        emit((OP_LOAD_GLOBAL, dest_reg, source_reg))

    def invoke():
        base_reg = next_register()
        argument_count = next_int()
        emit((OP_INVOKE, base_reg, argument_count))

    def op_return():
        base_reg = next_register()
        emit((OP_RETURN, base_reg, 0))

    def string():
        val = next()
        val = val[1:-1]
        constant((CONST_STRING, val))

    def nil():
        constant((CONST_NIL, None))

    def integer():
        val = next_int()
        constant((CONST_INT, val))

    def function():
        assembler['contexts'].append(new_context())
        assembler['contexts'][-2]['constants'].append((CONST_FUNC, assembler['contexts'][-1]))

    def endfunction():
        assembler['contexts'].pop()

    handlers = {
        'arity': arity,
        'locals': locals,
        'lc': load_constant,
        'lg': load_global,
        'invoke': invoke,
        'string': string,
        'function': function,
        'endfunction': endfunction,
        'return': op_return,
        'nil': nil,
        'integer': integer,
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
        def write_int(v):
            writeI64(v)
        handlers = {
            CONST_STRING: write_string,
            CONST_FUNC: write_function,
            CONST_NIL: write_nil,
            CONST_INT: write_int,
        }
        const_type, val = const
        writeU8(const_type[0])
        handler = handlers[const_type]
        handler(val)

    def write_function(func):
        writeU16(func['arity'])
        writeU16(func['locals'])
        writeU16(len(func['opcodes']))
        for ((op, _name), arg1, arg2) in func['opcodes']:
            arg1 = arg1 << 12
            full = op | arg1 | arg2
            writeU32(full)
        writeU16(len(func['constants']))
        for constant in func['constants']:
            write_constant(constant)

    write_function(assembler['contexts'][0])

    sys.stdout.buffer.write(assembler['output'])

if __name__ == '__main__':
    main()
