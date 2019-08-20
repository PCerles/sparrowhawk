import time
import re
import sys

import normalizer
import pywrapfst as fst
from functools import reduce
from multiprocessing import Pool


''' Script for running an arbitrary string through the tokenizer + verbalizer 
    and printing every possible verbalization of the string
'''
def construct_verbalizer(transcript):
    fst_string, unique_vocab = norm.construct_verbalizer_string(transcript)

    compiler = fst.Compiler()
    for line in fst_string.split('\n'): 
        print(line, file=compiler) 
    compiled = compiler.compile()
    compiled.write('a.fst')
    return compiled, unique_vocab


def make_lexicon_fst(input_words, words):
    compiler = fst.Compiler()
    lexicon_fst = fst.Fst()
    start = lexicon_fst.add_state()
    lexicon_fst.set_start(start)

    last = lexicon_fst.add_state()

    # this line projects space to epsilon
    lexicon_fst.add_arc(start, fst.Arc(32, 0, fst.Weight.One(lexicon_fst.weight_type()), last))
    lexicon_fst.set_final(last)
    for i, w in enumerate(words):
        w = w.strip()
        index = i + 1
        last = lexicon_fst.add_state()
        lexicon_fst.add_arc(start, fst.Arc(ord(w[0]), index, fst.Weight.One(lexicon_fst.weight_type()), last))
        for c in w[1:]:
            this = lexicon_fst.add_state()
            lexicon_fst.add_arc(last, fst.Arc(ord(c), 0, fst.Weight.One(lexicon_fst.weight_type()), this))
            last = this
        lexicon_fst.set_final(last, 0)
    lexicon_fst = fst.determinize(lexicon_fst).minimize().closure()

    with open('words.syms', 'w') as f:
        f.write('<eps> 0\n')
        for i, w in enumerate(words + input_words): # we put word symbol here
            f.write('{} {}'.format(w, str(i + 1)))
            f.write('\n')
        f.write('<SPACE> {}\n'.format(str(32)))

    epsilon_fst = fst.Fst()
    start = epsilon_fst.add_state()
    end = epsilon_fst.add_state()
    for i, w in enumerate(words):
        index = i + 1
        epsilon_fst.add_arc(start, fst.Arc(0, index, fst.Weight.One(epsilon_fst.weight_type()), end))

    epsilon_fst.add_arc(start, fst.Arc(0, 32, fst.Weight.One(epsilon_fst.weight_type()), end))
    epsilon_fst.set_final(end, 0)
    epsilon_fst.set_start(start)
    epsilon_fst = epsilon_fst.closure()

    return lexicon_fst, epsilon_fst

def get_trivial_fst(word_index):
    trivial_word_fst = fst.Fst()
    start = trivial_word_fst.add_state()
    end = trivial_word_fst.add_state()

    trivial_word_fst.set_start(start)
    trivial_word_fst.set_final(end, 0)

    trivial_word_fst.add_arc(start, fst.Arc(word_index, 0, fst.Weight.One(trivial_word_fst.weight_type()), end))
    return trivial_word_fst


def run(words):

    construct_verbalizer('here is a check for $40 and $5.95 and 10 puppy')
    words = ['10', '$1.95', '$50', 'hello', '$40', "new", "york", "110th"]

    with open('CNN_HD_2018-12-12_14-29-00.001.srt', 'r') as f:
        words = f.read().strip().split()

    big_verb_fst = None
    unique_vocab_set = set()

    verbalizers = []

    a = time.time()
    for w in words:
        verbalizer, unique_vocab = construct_verbalizer(w)
        for v in unique_vocab:
            unique_vocab_set.add(v)
        verbalizers.append(verbalizer)
    print(time.time() - a, 'seconds for verbalizers')
    unique_vocab_set = list(unique_vocab_set)

    a = time.time()
    lexicon_fst, epsilon_fst = make_lexicon_fst(words, unique_vocab_set)
    print(time.time() - a, 'seconds for lexicon')

    unique_vocab_set += words

    big_verb_fst = None
    a = time.time()
    for w, v in zip(words, verbalizers):
        composed = fst.compose(v.project().arcsort(sort_type='olabel'), lexicon_fst).project(project_output=True)
        composed = fst.compose(epsilon_fst.arcsort(sort_type='olabel'), composed)
        trivial_word_fst = get_trivial_fst(unique_vocab_set.index(w) + 1)
        concat = fst.determinize(trivial_word_fst.concat(composed).rmepsilon().invert()).minimize()

        if big_verb_fst is None:
            big_verb_fst = concat
        else:
            big_verb_fst = big_verb_fst.union(concat)
    big_verb_fst = big_verb_fst.rmepsilon()
    print(time.time() - a, 'seconds for compositions')


    big_verb_fst.write('a.fst')
    return

#    verbalizer = verbalizer.project()
    #verbalizer.write('/home/philip/graves_loss/hive-speech/alignment/src/test.fst')

    verbalizer = fst.compose(verbalizer, space_deduper)#.project(project_output=True).rmepsilon()
    verbalizer.write('check.fst')
    verbalizer.write('/home/philip/graves_loss/hive-speech/alignment/src/test.fst')

    #verbalizer.write('out.fst')


def print_expansion(verbalizer):
    """has bugs
    #############################
    #                           #
    #     Print expansions      #
    #                           #
    ############################$
    """

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


    print('{}\n{}\n\n'.format(transcript, out_string))

if __name__ == '__main__':
    norm = normalizer.Normalizer()
    norm.setup("sparrowhawk_configuration.ascii_proto", "/workspace/sparrowhawk/documentation/grammars/")

    with open(sys.argv[1], 'r') as f:
        to_run = f.read()
    run(to_run)
