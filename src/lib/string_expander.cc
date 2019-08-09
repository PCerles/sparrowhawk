#include <sparrowhawk/string_expander.h>
#include <stdio.h>
#include <memory>
#include <string>
using std::string;

#include <google/protobuf/text_format.h>
#include <sparrowhawk/items.pb.h>
#include <sparrowhawk/sentence_boundary.h>
#include <sparrowhawk/serialization_spec.pb.h>
#include <sparrowhawk/sparrowhawk_configuration.pb.h>
#include <sparrowhawk/io_utils.h>
#include <sparrowhawk/logger.h>
#include <sparrowhawk/protobuf_parser.h>
#include <sparrowhawk/protobuf_serializer.h>
#include <sparrowhawk/spec_serializer.h>
#include <sparrowhawk/string_utils.h>

#include <arcsort.h>
#include <compose.h>

namespace speech {
namespace sparrowhawk {


bool only_spaces(const string& str) {
   return str.find_first_not_of(' ') == str.npos;
}

StringExpander::StringExpander(Fst * _verbalizer) : verbalizer(_verbalizer) {}

StringExpander::~StringExpander() {}

void StringExpander::expand(string& transcript, Fst * output, string delim = " ") {
    Fst transcript_lm;

    // Separate words
    std::vector<string> words;
    string word;
    for (char c : transcript) {
        if (c == delim) {
            if (!only_spaces(word)) words.push_back(word);
            word.clear();
        } else {
            word += c;
        }
    }

    // Build single transcript language model
    int  index = 0;
    auto it = words.begin();
    for (; it != words.end(); ++it, ++index) {
        n_it = it + 1;

        transcript_lm->AddState();
        
        if (n_it != words.end()) {
            transcript_lm->AddArc(index, StdArc(*it, *n_it, 0.0, index+1))
        } 
    }

    transcript_lm->SetStart(0);
    transcript_lm->SetFinal(words.size()-1);

    // Sort output labels
    ArcSort(&transcript_lm, OLabelCompare<Src>());


    // sort input labels of verbalizer
    Fst sorted_verbalizer = ArcSortFst<Arc, ILabelCompare<Arc> >(this.verbalizer, ILabelCompare<Arc>());

    // Compose and write to output   
    Compose(transcript_lm, sorted_verbalizer, output);

    if (this.DEBUG) {
        format_and_save_fst(output, "composed");
        format_and_save_fst(&transcript_lm, "transcript");
    }   
}


void StringExpander::format_and_save_fst(Fst * fst, string name) {
    char * buf[100];
    sprintf(buf, "%s/%s.fst", this.IMAGE_DIR, name);
    fst->write(buf);
}

