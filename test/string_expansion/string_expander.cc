#include <string>
#include <fst/fstlib.h>
#include <fst/fst-decl.h>
#include <fst/arc.h>
#include <fst/arcsort.h>
#include <fst/compose.h>
#include <fst/closure.h>

using std::string;
using fst::MutableFst;
    
typedef fst::StringCompiler<fst::StdArc> Compiler;

namespace speech {
namespace alignment {

// StringExpander<Arc> decl

template <class Arc>
class StringExpander {
    static bool DEBUG;
    static char const * IMAGE_DIR;

  public:
    explicit StringExpander(MutableFst<Arc> * _verbalizer);

    virtual ~StringExpander();

    void expand(string& transcript, MutableFst<Arc>* output, char delim=' ');

    static void format_and_save_fst(MutableFst<Arc>* fst, char const * name);


  private:
    MutableFst<Arc> * verbalizer;

};

// StringExpander<Arc> decl

template <class Arc>
StringExpander<Arc>::StringExpander(MutableFst<Arc> * _verbalizer) : verbalizer(_verbalizer) {}

template <class Arc>
StringExpander<Arc>::~StringExpander() {}

template <class Arc>
void StringExpander<Arc>::expand(string& transcript, MutableFst<Arc> * output, char delim ) {
    // Build transcript fst
    Compiler compiler(fst::StringTokenType::BYTE);
    fst::VectorFst<fst::StdArc> transcript_lm;
    if (!compiler(transcript, &transcript_lm)) {
        printf("Failed to compile input string \"%s\"", transcript.c_str());
        return;
    }

    // Sort labels
    ArcSort(&transcript_lm, fst::OLabelCompare<Arc>());
    ArcSort(verbalizer, fst::ILabelCompare<Arc>());

    Closure(verbalizer, fst::CLOSURE_PLUS);

    // Compose and write to output   
    fst::Compose<Arc>(transcript_lm, *verbalizer, output);

    if (DEBUG) {
        format_and_save_fst(output, "composed");
        format_and_save_fst(&transcript_lm, "transcript");
        format_and_save_fst(verbalizer, "verbalizer");
    }   
}

template <class Arc>
void StringExpander<Arc>::format_and_save_fst(MutableFst<Arc> * fst, char const * name) {
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


int main(void) {

    string TRANSCRIPT = "11";

    fst::VectorFst<fst::StdArc> verbalizer;

    // Build verbalizer
    verbalizer.AddState();
    verbalizer.AddState();
    verbalizer.AddState();
    verbalizer.AddState();

    verbalizer.AddArc(0, fst::StdArc('1', 'o', 0.0, 1));
    verbalizer.AddArc(1, fst::StdArc(0, 'n', 0.0, 2));
    verbalizer.AddArc(2, fst::StdArc(0, 'e', 0.0, 3));
    
    verbalizer.SetStart(0);
    verbalizer.SetFinal(3, 0.0);


    // Expand
    speech::alignment::StringExpander<fst::StdArc> expander(&verbalizer);

    fst::VectorFst<fst::StdArc> output;
    expander.expand(TRANSCRIPT, &output);
}
