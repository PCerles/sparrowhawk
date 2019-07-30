with open('bytes.syms', 'r') as f:
    for line in f:
        sym, number = line.split('\t')
        if sym == '<SPACE>':
            print('0 1 {} {}'.format(sym, sym))
            print('1 1 {} {}'.format(sym, '<eps>'))
        else:
            if sym != '<eps>': 
                out_sym = sym
                print('0 0 {} {}'.format(sym, out_sym))
                print('1 0 {} {}'.format(sym, out_sym))
    print(0)
    
