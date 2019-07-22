import normalizer
import pywrapfst as fst
import sys

''' Script for running an arbitrary string through the tokenizer + verbalizer 
    and printing every possible verbalization of the string
'''


def construct_verbalizer(transcript):
    compiler = fst.Compiler()
    fst_string = normalizer.construct_acceptor(transcript)
    for line in fst_string.split('\n'): 
        print >> compiler, line

    return compiler.compile()

for line in sys.stdin:
    transcript = line.strip('\n')

    verbalizer = construct_verbalizer(transcript)


    #############################
    #                           #
    #     Print expansions      #
    #                           #
    ############################$

    # Allocate graph
    NODES   = [dict() for _ in range(verbalizer.num_states())]
    VISITED = [False  for _ in range(verbalizer.num_states())]

    # Build graph
    for state in verbalizer.states():
       for arc in verbalizer.arcs(state):
            NODES[state][arc.ilabel]=arc.nextstate

    # Returns path where an arc is represented as (dest_node_index, arc_char)
    def get_all_paths(src, dest):
        all_paths = []
        def _get_all_paths(src, dest, path):
            VISITED[src]=True
            if src == dest: 
                all_paths.append(path)
                return
            
            for char, adj in NODES[src].items():
                to_add = (adj, chr(char))
                if not VISITED[adj]: _get_all_paths(adj, dest, path + [to_add])
                VISITED[adj]=False
            
        _get_all_paths(src, dest, [])
        VISITED[src]=False
        return all_paths

    paths = get_all_paths(0, verbalizer.num_states()-1)

    # Nodes that show up in all paths separate tokens 
    seps = reduce(lambda x,y: set(x) & set(y), paths)
    seps = sorted(list([s[0] for s in seps]))
    seps = [0] + seps

    # list of all possible sub paths between consecutive nodes that appear in all full paths
    tokens = [get_all_paths(seps[i], seps[i+1]) for i in range(len(seps)-1)]

    # Format output string
    out_string = ""
    for expansions in tokens:
        strings = [''.join([c[1] for c in s]) for s in expansions]
        out_string += " ( {}) " if len(expansions) > 1 else "{}"
        out_string = out_string.format(' | '.join(strings))

    print('\n\n{}'.format(out_string))


