#include <string>
#include <fst/fstlib.h>
#include <fst/fst-decl.h>
#include <fst/arc.h>
#include <fst/arcsort.h>
#include <fst/compose.h>

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

//bool only_spaces(const string& str) {
//   return str.find_first_not_of(' ') == str.npos;
//}

template <class Arc>
StringExpander<Arc>::StringExpander(MutableFst<Arc> * _verbalizer) : verbalizer(_verbalizer) {}

template <class Arc>
StringExpander<Arc>::~StringExpander() {}

template <class Arc>
void StringExpander<Arc>::expand(string& transcript, MutableFst<Arc> * output, char delim ) {
    //MutableFst<Arc> transcript_lm;

    //// Separate words
    //std::vector<string> words;
    //string word;
    //for (char c : transcript) {
    //    if (c == delim) {
    //        //if (!only_spaces(word))
    //        words.push_back(word);
    //        word.clear();
    //    } else {
    //        word += c;
    //    }
    //}

    Compiler compiler(fst::StringTokenType::BYTE);
    fst::VectorFst<fst::StdArc> transcript_lm;
    if (!compiler(transcript, &transcript_lm)) {
        printf("Failed to compile input string \"%s\"", transcript.c_str());
        return;
    }

    //// Build single transcript language model
    //int  index = 0;
    //auto it = words.begin();
    //for (; it != words.end(); ++it, ++index) {
    //    auto n_it = it + 1;

    //    transcript_lm->AddState();
    //    
    //    if (n_it != words.end()) {
    //        transcript_lm->AddArc(index, Arc(*it, *n_it, 0.0, index+1));
    //    } 
    //}

    //transcript_lm->SetStart(0);
    //transcript_lm->SetFinal(words.size()-1);

    // Sort output labels
    //ArcSort(&transcript_lm, fst::OLabelCompare<Arc>());
    // ArcSort(&transcript_lm, fst::ILabelCompare<Arc>());


    // sort input labels of verbalizer
    //auto sorted_verbalizer = fst::ArcSortFst<Arc, fst::ILabelCompare<Arc> >(*verbalizer, fst::ILabelCompare<Arc>());

    ArcSort(verbalizer, fst::ILabelCompare<Arc>());
    // ArcSort(verbalizer, fst::OLabelCompare<Arc>());

    // Compose and write to output   
    fst::Compose<Arc>(transcript_lm, *verbalizer, output);
    //fst::Compose<Arc>(*verbalizer, transcript_lm, output);

    if (DEBUG) {
        format_and_save_fst(output, "composed");
        format_and_save_fst(&transcript_lm, "transcript");
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

    string transcript = "test 1";

    fst::VectorFst<fst::StdArc> verbalizer;

    // Build verbalizer
    verbalizer.AddState();
    verbalizer.AddState();
    verbalizer.AddState();
    verbalizer.AddState();
//    verbalizer.AddState();
//    verbalizer.AddState();

    verbalizer.AddArc(0, fst::StdArc('1', 'o', 0.0, 1));
    verbalizer.AddArc(1, fst::StdArc(0, 'n', 0.0, 2));
    verbalizer.AddArc(2, fst::StdArc(0, 'e', 0.0, 3));

//    verbalizer.AddArc(0, fst::StdArc('1', 'a', 0.0, 4));
//    verbalizer.AddArc(4, fst::StdArc(0, 'b', 0.0, 5));
//    verbalizer.AddArc(5, fst::StdArc(0, 'c', 0.0, 3));
//
    verbalizer.SetStart(0);
    verbalizer.SetFinal(3, 0.0);

    speech::alignment::StringExpander<fst::StdArc>::format_and_save_fst(&verbalizer, "test_verb");

    speech::alignment::StringExpander<fst::StdArc> expander(&verbalizer);

    fst::VectorFst<fst::StdArc> output;
    expander.expand(transcript, &output);
}
