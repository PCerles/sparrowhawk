#include <string.h>
#include <sparrowhawk/io_utils.h>
#include <sparrowhawk/sparrowhawk_configuration.pb.h>
#include <sparrowhawk/rule_system.h>
#include <fst/fstlib.h>
#include <fst/fst-decl.h>
#include <fst/arc.h>
#include <fst/arcsort.h>
#include <fst/compose.h>
#include <fst/closure.h>
#include <fst/symbol-table.h>
#include <assert.h>

using std::string;
using fst::MutableFst;
using fst::SymbolTable;

typedef fst::StringCompiler<fst::StdArc> Compiler;

namespace speech {
namespace alignment {

// StringExpander<Arc> decl

template <class Arc>
class StringExpander {
    static bool DEBUG;
    static char const * IMAGE_DIR;

  public:
    explicit StringExpander(const string &configuration_proto,
                                const string &pathname_prefix, 
                                SymbolTable * _symbol_table = nullptr);

    virtual ~StringExpander();

    void expand(string transcript, MutableFst<Arc>* output, char delim=' ');

    static void format_and_save_fst(fst::Fst<Arc>* fst, char const * name);


  private:
    MutableFst<Arc> * verbalizer;
    SymbolTable * symbol_table;
    std::unique_ptr<sparrowhawk::RuleSystem> tokenizer_classifier_rules_;
    std::unique_ptr<sparrowhawk::RuleSystem> verbalizer_rules_;
//    std::unique_ptr<sparrowhawk::SentenceBoundary> sentence_boundary_;
//    std::unique_ptr<sparrowhawk::Serializer> spec_serializer_;
//    std::set<string> sentence_boundary_exceptions_;

};

// StringExpander<Arc> decl

template <class Arc>
StringExpander<Arc>::StringExpander(
                                    const string &configuration_proto,
                                    const string &pathname_prefix,
                                    SymbolTable * _symbol_table ) 
    : symbol_table(_symbol_table)
{
    sparrowhawk::SparrowhawkConfiguration configuration;
    string proto_string = sparrowhawk::IOStream::LoadFileToString(pathname_prefix +
                                                     "/" + configuration_proto);
    if (!google::protobuf::TextFormat::ParseFromString(proto_string, &configuration))
      throw("!google::protobuf::TextFormat::ParseFromString(proto_string, &configuration) failed");
    if (!(configuration.has_tokenizer_grammar()))
      throw("Configuration does not define a tokenizer-classifier grammar");
    if (!(configuration.has_verbalizer_grammar()))
      throw("Configuration does not define a verbalizer grammar");
    tokenizer_classifier_rules_.reset(new sparrowhawk::RuleSystem);
    if (!tokenizer_classifier_rules_->LoadGrammar(
            configuration.tokenizer_grammar(),
            pathname_prefix))
      throw("tokenizer rules failed");
    verbalizer_rules_.reset(new sparrowhawk::RuleSystem);
    if (!verbalizer_rules_->LoadGrammar(configuration.verbalizer_grammar(),
                                        pathname_prefix))
      throw("verbalizer rules failed");
    //string sentence_boundary_regexp;
    //if (configuration.has_sentence_boundary_regexp()) {
    //  sentence_boundary_regexp = configuration.sentence_boundary_regexp();
    //} else {
    //  sentence_boundary_regexp = kDefaultsparrowhawk::SentenceBoundaryRegexp;
    //}
    //sentence_boundary_.reset(new sparrowhawk::SentenceBoundary(sentence_boundary_regexp));
    //if (configuration.has_sentence_boundary_exceptions_file()) {
    //  if (!sentence_boundary_->Loadsparrowhawk::SentenceBoundaryExceptions(
    //          configuration.sentence_boundary_exceptions_file())) {
    //    throw("Cannot load sentence boundary exceptions file: " + 
    //                configuration.sentence_boundary_exceptions_file().c_str());
    //  }
    //}
    //if (configuration.has_serialization_spec()) {
    //  string spec_string = sparrowhawk::IOStream::LoadFileToString(
    //      pathname_prefix + "/" + configuration.serialization_spec());
    //  SerializeSpec spec;
    //  if (spec_string.empty() ||
    //      !google::protobuf::TextFormat::ParseFromString(spec_string, &spec) ||
    //      (spec_serializer_ = sparrowhawk::Serializer::Create(spec)) == nullptr) {
    //    throw("Failed to load a valid serialization spec from file: " + configuration.serialization_spec());
    //  }
    //}
}





template <class Arc>
StringExpander<Arc>::~StringExpander() {}

template <class Arc>
void StringExpander<Arc>::expand(string transcript, MutableFst<Arc> * output, char delim ) {
    // Build transcript fst
    Compiler compiler(fst::StringTokenType::SYMBOL, symbol_table);
    fst::VectorFst<fst::StdArc> transcript_lm;
    transcript_lm.SetOutputSymbols(symbol_table);
    transcript_lm.SetInputSymbols(symbol_table);
    if (!compiler(transcript, &transcript_lm)) {
        printf("Failed to compile input string \"%s\"", transcript.c_str());
        return;
    }

    // Tokenize
    fst::VectorFst<fst::StdArc> tok_output;

    if (!tokenizer_classifier_rules_->ApplyRules(transcript_lm,
                                                 &tok_output,
                                                 true /*  use_lookahead */)) {
      std::cout << "Failed to tokenize " << transcript << std::endl;
      return;
    }

    fst::VectorFst<fst::StdArc> shortest_path;
    fst::ShortestPath(tok_output, &shortest_path);
    //ProtobufParser parser(&shortest_path);
    //if (!parser.ParseTokensFromFST(utt, true /* set SEMIOTIC_CLASS */)) {
    //  LoggerError("Failed to parse tokens from FST for \"%s\"", input.c_str());
    //  return false;
    //}

    format_and_save_fst(&shortest_path, "shortest_path");

  



//    // Sort labels
//    ArcSort(&transcript_lm, fst::OLabelCompare<Arc>());
//    ArcSort(verbalizer, fst::ILabelCompare<Arc>());
//
//    Closure(verbalizer, fst::CLOSURE_PLUS);
//
//    // Compose and write to output   
//    fst::Compose<Arc>(transcript_lm, *verbalizer, output);
//
//    if (DEBUG) {
//        format_and_save_fst(output, "composed");
//        format_and_save_fst(&transcript_lm, "transcript");
//        format_and_save_fst(verbalizer, "verbalizer");
//    }   
}

template <class Arc>
void StringExpander<Arc>::format_and_save_fst(fst::Fst<Arc> * fst, char const * name) {
    char buf[100];
    sprintf(buf, "%s/%s.fst", IMAGE_DIR, name);
    fst->Write(buf);
    std::cout << name << " written" << std::endl;
}

  
template <class Arc>
bool StringExpander<Arc>::DEBUG = true;


template <class Arc>
char const * StringExpander<Arc>::IMAGE_DIR = "/tts/images/";


} // namespace alignment
} // namespace speech



