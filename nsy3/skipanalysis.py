from .bytecode import Bytecode, Combine

RETURN_SKIP = 2 ** 16 - 1

REMOVES = {
    Bytecode.GETATTR: lambda code: 2,
    Bytecode.CALL: lambda code: code.arg_v + 1,
    Bytecode.BINOP: lambda code: 2,
    Bytecode.GET: lambda code: 0,
    Bytecode.SET: lambda code: 1,
    Bytecode.CONST: lambda code: 0,
    Bytecode.JUMP: lambda code: 0,
    Bytecode.JUMP_IF: lambda code: 1,
    Bytecode.JUMP_IFNOT: lambda code: 1,
    Bytecode.JUMP_IF_KEEP: lambda code: 1,
    Bytecode.JUMP_IFNOT_KEEP: lambda code: 1,
    Bytecode.DROP: lambda code: code.arg_v,
    Bytecode.RETURN: lambda code: 1,
    Bytecode.GETENV: lambda code: 0,
    Bytecode.DUP: lambda code: 1,
    Bytecode.ROT: lambda code: code.arg_v,
    Bytecode.RROT: lambda code: code.arg_v,
    Bytecode.BUILDLIST: lambda code: code.arg_v,
    Bytecode.UNPACK: lambda code: 1,
    Bytecode.LABEL: lambda code: 0,
    Bytecode.LINENO: lambda code: 0,
    Bytecode.IGNORE: lambda code: 0,
}

ADDS = {
    Bytecode.GETATTR: lambda code: 1,
    Bytecode.CALL: lambda code: 1,
    Bytecode.BINOP: lambda code: 1,
    Bytecode.GET: lambda code: 1,
    Bytecode.SET: lambda code: 0,
    Bytecode.CONST: lambda code: 1,
    Bytecode.JUMP: lambda code: 0,
    Bytecode.JUMP_IF: lambda code: 0,
    Bytecode.JUMP_IFNOT: lambda code: 0,
    Bytecode.JUMP_IF_KEEP: lambda code: 1,
    Bytecode.JUMP_IFNOT_KEEP: lambda code: 1,
    Bytecode.DROP: lambda code: 0,
    Bytecode.RETURN: lambda code: 0,
    Bytecode.GETENV: lambda code: 1,
    Bytecode.DUP: lambda code: 1 + code.arg_v,
    Bytecode.ROT: lambda code: code.arg_v,
    Bytecode.RROT: lambda code: code.arg_v,
    Bytecode.BUILDLIST: lambda code: 1,
    Bytecode.UNPACK: lambda code: code.arg_v.a,
    Bytecode.LABEL: lambda code: 0,
    Bytecode.LINENO: lambda code: 0,
    Bytecode.IGNORE: lambda code: 0,
}

SKIP_NOT_REQUIRED = {Bytecode.CONST, Bytecode.GETENV, Bytecode.BUILDLIST, Bytecode.UNPACK, Bytecode.ROT, Bytecode.RROT, Bytecode.DUP, Bytecode.JUMP_IF_KEEP, Bytecode.JUMP_IFNOT_KEEP}

# Essence of this algorithm: (good luck!)
#   We want to find, for each instruction, the nearest (in the graph sense) instruction that is:
#     - executed after the current on in all possible paths through the code
#     - not affected by the results of the current instruction (i.e. nothing on the stack comes from the current instruction or a dependent instruction)
#   This shall be referred to as the "skip instruction"
#
#   Other notes:
#     - the stack itself cannot have thunks, so skips only occur when an instruction tries to push something
#
#   Therefore we calculate the post-dominating instructions for each instruction, as well as the possible stacks when that function
#   is executed, so that we can find an instruction that satisfied the above rules.


def get_control_flow_graph(lin_code):
    next_nodes = []

    for pos, code in enumerate(lin_code):
        if code.type is Bytecode.JUMP:
            next_nodes.append([lin_code.index(code.arg_v)])
        elif code.type in (Bytecode.JUMP_IF, Bytecode.JUMP_IFNOT, Bytecode.JUMP_IF_KEEP, Bytecode.JUMP_IFNOT_KEEP):
            next_nodes.append([pos+1, lin_code.index(code.arg_v)])
        elif code.type is Bytecode.RETURN:
            next_nodes.append([None])
        else:
            next_nodes.append([pos+1])

    return next_nodes



def get_possible_stacks_and_args(lin_code, control_flow_graph):
    possible_stacks = [set() for _ in lin_code]
    possible_args = [set() for _ in lin_code]

    # Search over all possible stack setups
    # Stack of (position, stack)
    todo = [(0, ())]
    while todo:
        pos, stack = todo.pop()
        if stack in possible_stacks[pos]:
            continue
        possible_stacks[pos].add(stack)
        code = lin_code[pos]
        remove = REMOVES[code.type](code)
        add = ADDS[code.type](code)
        possible_args[pos].add(stack[-remove:] if remove else ())
        new_stack = (stack[:-remove] if remove else stack) + (pos,) * add

        if len(stack) < remove:
            raise RuntimeError("Not enough on the stack")

        for next_node in control_flow_graph[pos]:
            if next_node is not None:
                todo.append((next_node, new_stack))

    return possible_stacks, possible_args


def get_dependent_instructions(possible_stacks):
    depends = [set() for _ in possible_stacks]
    changed = True
    while changed:
        changed = False
        for pos in range(len(depends)):
            len_depends = len(depends[pos])
            for stack in possible_stacks[pos]:
                depends[pos].update(stack)
                for value in stack:
                    depends[pos].update(depends[value])
            changed = changed or len(depends[pos]) != len_depends
    return depends


def get_post_dominating_nodes(control_flow_graph):
    all_nodes = set(range(len(control_flow_graph)))
    all_nodes.add(None)
    post_dominators = [all_nodes.copy() for _ in control_flow_graph]

    changed = True
    while changed:
        changed = False
        for pos in range(len(post_dominators)):
            len_dom = len(post_dominators[pos])
            new_nodes = all_nodes.copy()
            for next_node in control_flow_graph[pos]:
                new_nodes &= post_dominators[next_node] if next_node is not None else {None}
            new_nodes.add(pos)
            post_dominators[pos] = new_nodes
            changed = changed or len(post_dominators[pos]) != len_dom

    return post_dominators


def get_reachable_instrs(pos, control_flow_graph):
    # Stack of positions
    todo = [pos]
    reached = []
    while todo:
        pos = todo.pop()
        if pos in reached:
            continue
        reached.append(pos)

        for next_node in control_flow_graph[pos]:
            if next_node is not None:
                todo.append(next_node)

    reached.append(None)
    return reached


def get_possibly_set_variables(pos, skip_pos, control_flow_graph, lin_code):
    # Stack of positions
    todo = [pos]
    done = set()
    variables = set()
    while todo:
        pos = todo.pop()
        if pos in done:
            continue
        done.add(pos)
        if lin_code[pos].type is Bytecode.SET:
            variables.add(lin_code[pos].arg_v)

        for next_node in control_flow_graph[pos]:
            if next_node is not None and next_node != skip_pos:
                todo.append(next_node)

    return variables


def isprefix(seq, prefix):
    return len(seq) >= len(prefix) and seq[:len(prefix)] == prefix


def get_skip_pos(pos, lin_code, control_flow_graph, post_dominators, depend_instrs, possible_stacks):
    item = lin_code[pos]
    reachable = get_reachable_instrs(pos, control_flow_graph)
    for dom_pos in sorted(post_dominators[pos], key=reachable.index):
        if dom_pos != pos and (dom_pos is None or pos not in depend_instrs[dom_pos]):
            prefix_length = set()
            for stack in possible_stacks[pos]:
                if rm := REMOVES[item.type](item):
                    stack = stack[:-rm]
                skip_stacks = possible_stacks[dom_pos] if dom_pos is not None else [()]
                if not any(isprefix(stack, skip_stack) for skip_stack in skip_stacks):
                    break
                prefix_length.update(len(stack) - len(skip_stack) for skip_stack in skip_stacks)
            else:
                assert len(prefix_length) < 2
                prefix_length = prefix_length.pop() if prefix_length else 0

                return dom_pos, prefix_length, get_possibly_set_variables(pos, dom_pos, control_flow_graph, lin_code)
    raise RuntimeError(f"Could not find a skip pos for {pos}")


def skipanalysis(bcode):
    lin_code = bcode.linearize()
    control_flow_graph = get_control_flow_graph(lin_code)
    possible_stacks, possible_args = get_possible_stacks_and_args(lin_code, control_flow_graph)
    depend_instrs = get_dependent_instructions(possible_stacks)
    post_dominators = get_post_dominating_nodes(control_flow_graph)

    skipping_lin_code = lin_code.copy()
    for pos, item in reversed(list(enumerate(lin_code))):
        if ADDS[item.type](item) and item.type not in SKIP_NOT_REQUIRED:
            skip_pos, stack_drop, skip_vars = get_skip_pos(pos, lin_code,control_flow_graph, post_dominators, depend_instrs, possible_stacks)
            skipping_lin_code.insert(pos, Bytecode.SETSKIP(Combine(lin_code[skip_pos] if skip_pos is not None else RETURN_SKIP, stack_drop)))
            for var in skip_vars:
                skipping_lin_code.insert(pos+1, Bytecode.SKIPVAR(var))
    return Bytecode.SEQ(*skipping_lin_code)
