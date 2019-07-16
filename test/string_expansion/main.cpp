#include "string_expander.h"

std::string CONFIGURATION_PROTO = "sparrowhawk_configuration.ascii_proto";
std::string PATH_PREFIX = "/tts/sparrowhawk/documentation/grammars/";


int main(void) {

    char const * TRANSCRIPT = "I ATE 10 DONUTS";

    fst::VectorFst<fst::StdArc> verbalizer;

    SymbolTable symbol_table("test.syms");

    assert (symbol_table.AddSymbol("<eps>") == 0);
    assert (symbol_table.AddSymbol("I")  ==  1);
    assert (symbol_table.AddSymbol("ATE") == 2);
    assert (symbol_table.AddSymbol("TEN") == 3);
    assert (symbol_table.AddSymbol("10") == 4);
    assert (symbol_table.AddSymbol("ONE") == 5);
    assert (symbol_table.AddSymbol("ZERO") == 6);
    assert (symbol_table.AddSymbol("DONUTS") == 7);
    assert (symbol_table.AddSymbol(" ") == 8);
    
    //Compiler compiler(fst::StringTokenType::SYMBOL, &symbol_table);

    fst::VectorFst<fst::StdArc> transcript_lm;
    transcript_lm.SetOutputSymbols(&symbol_table);
    transcript_lm.SetInputSymbols(&symbol_table);


    transcript_lm.AddState();
    transcript_lm.AddState();

    transcript_lm.AddArc(0, fst::StdArc(symbol_table.Find("ATE"), symbol_table.Find("ATE"), 0.0, 1));

    transcript_lm.SetStart(0);
    transcript_lm.SetFinal(1, 0.0);


//    if (!compiler(TRANSCRIPT, &transcript_lm)) {
//        printf("Failed to compile input string \"%s\"", TRANSCRIPT);
//        return 0;
//    }


    speech::alignment::StringExpander<fst::StdArc>::format_and_save_fst(&transcript_lm, "transcript");

    // Expand
    speech::alignment::StringExpander<fst::StdArc> expander(CONFIGURATION_PROTO, PATH_PREFIX);

    expander.expand(TRANSCRIPT, nullptr);

//    fst::VectorFst<fst::StdArc> output;
//    expander.expand(TRANSCRIPT, &output);

}
